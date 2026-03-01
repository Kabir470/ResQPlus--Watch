/*
 * BLE GATT Server – Legacy BLE 4.2 Advertising
 * Uses Nordic UART Service (NUS) for sending alerts to a connected phone.
 * Memory-optimised: BLE 5.0 extended features are disabled to save ~30 KB
 * of internal SRAM so that WiFi + LCD DMA can coexist.
 */

#include "ble_server.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define GATTS_TAG "BLE_SERVER"

/* ---- UUIDs matching the React Native app (128-bit, little-endian) ----
 *  Service:   4fafc201-1fb5-459e-8fcc-c5c9c331914b
 *  TX (notify): beb5483e-36e1-4688-b7f5-ea07361b26a8  (ALERT_CHAR_UUID in app)
 *  RX (write):  beb5483f-36e1-4688-b7f5-ea07361b26a8
 */
static uint8_t nus_service_uuid[16] = {
    0x4B, 0x91, 0x31, 0xC3, 0xC9, 0xC5, 0xCC, 0x8F,
    0x9E, 0x45, 0xB5, 0x1F, 0x01, 0xC2, 0xAF, 0x4F};
static uint8_t nus_rx_char_uuid[16] = {
    0xA8, 0x26, 0x1B, 0x36, 0x07, 0xEA, 0xF5, 0xB7,
    0x88, 0x46, 0xE1, 0x36, 0x3F, 0x48, 0xB5, 0xBE};
static uint8_t nus_tx_char_uuid[16] = {
    0xA8, 0x26, 0x1B, 0x36, 0x07, 0xEA, 0xF5, 0xB7,
    0x88, 0x46, 0xE1, 0x36, 0x3E, 0x48, 0xB5, 0xBE};

#define TEST_DEVICE_NAME "ESP32_S3_WATCH"
#define GATTS_NUM_HANDLE 8 /* service(1) + rx_char_decl(1) + rx_char_val(1) + tx_char_decl(1) + tx_char_val(1) + tx_cccd(1) + spare(2) */

/* ---- Connection state ---- */
static bool is_connected = false;
static uint16_t conn_id = 0;
static esp_gatt_if_t gatts_if_handle = ESP_GATT_IF_NONE;
static uint16_t tx_char_handle = 0;

/* ---- Connection approval state ---- */
static volatile bool pending_approval = false; /* prompt shown, waiting for user */
static volatile bool user_approved = false;    /* user tapped Accept */
static uint8_t pending_bda[6] = {0};
static volatile bool ble_enabled = true;

/* ---- Advertising data (legacy BLE 4.2) ---- */
static uint8_t adv_config_done = 0;
#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(nus_service_uuid),
    .p_service_uuid = nus_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x40, /* 40 ms */
    .adv_int_max = 0x80, /* 80 ms */
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ---- GATTS profile table ---- */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    esp_bt_uuid_t char_uuid;
};

static struct gatts_profile_inst gl_profile = {
    .gatts_cb = gatts_profile_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
};

/* ================================================================
 *  GAP callback – legacy advertising
 * ================================================================ */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
        }
        else
        {
            ESP_LOGI(GATTS_TAG, "Advertising started");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(GATTS_TAG, "Advertising stopped");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "conn params update: status=%d, int=%d, latency=%d, timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    /* ---- Security / Pairing events ---- */
    case ESP_GAP_BLE_SEC_REQ_EVT:
        /* Phone requested pairing – accept it */
        ESP_LOGI(GATTS_TAG, "SEC_REQ from %02x:%02x:%02x:%02x:%02x:%02x",
                 param->ble_security.ble_req.bd_addr[0],
                 param->ble_security.ble_req.bd_addr[1],
                 param->ble_security.ble_req.bd_addr[2],
                 param->ble_security.ble_req.bd_addr[3],
                 param->ble_security.ble_req.bd_addr[4],
                 param->ble_security.ble_req.bd_addr[5]);
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
    {
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTS_TAG, "AUTH_CMPL addr=%02x:%02x:%02x:%02x:%02x:%02x  success=%d",
                 bd_addr[0], bd_addr[1], bd_addr[2],
                 bd_addr[3], bd_addr[4], bd_addr[5],
                 param->ble_security.auth_cmpl.success);
        if (!param->ble_security.auth_cmpl.success)
        {
            ESP_LOGW(GATTS_TAG, "Pairing failed, reason 0x%x",
                     param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    }

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        ESP_LOGI(GATTS_TAG, "Passkey: %06" PRIu32, param->ble_security.key_notif.passkey);
        break;

    case ESP_GAP_BLE_NC_REQ_EVT:
        /* Numeric comparison – auto-accept (Just Works) */
        ESP_LOGI(GATTS_TAG, "NC confirm: %06" PRIu32, param->ble_security.key_notif.passkey);
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;

    case ESP_GAP_BLE_KEY_EVT:
        ESP_LOGI(GATTS_TAG, "KEY type=%d", param->ble_security.ble_key.key_type);
        break;

    default:
        break;
    }
}

/* ================================================================
 *  GATTS profile callback
 * ================================================================ */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {

    /* ---------- APP registered ---------- */
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "REG_EVT app_id %d", param->reg.app_id);

        gl_profile.service_id.is_primary = true;
        gl_profile.service_id.id.inst_id = 0x00;
        gl_profile.service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile.service_id.id.uuid.uuid.uuid128, nus_service_uuid, 16);

        esp_ble_gap_set_device_name(TEST_DEVICE_NAME);

        /* Configure legacy advertising data */
        esp_ble_gap_config_adv_data(&adv_data);
        adv_config_done |= ADV_CONFIG_FLAG;

        esp_ble_gatts_create_service(gatts_if, &gl_profile.service_id, GATTS_NUM_HANDLE);
        break;

    /* ---------- Service created ---------- */
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "SERVICE_CREATE handle %d", param->create.service_handle);
        gl_profile.service_handle = param->create.service_handle;

        esp_ble_gatts_start_service(gl_profile.service_handle);

        /* Add RX characteristic (phone writes to watch) */
        gl_profile.char_uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile.char_uuid.uuid.uuid128, nus_rx_char_uuid, 16);
        esp_ble_gatts_add_char(gl_profile.service_handle,
                               &gl_profile.char_uuid,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                               NULL, NULL);
        break;

    /* ---------- Characteristic added ---------- */
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "ADD_CHAR attr_handle %d", param->add_char.attr_handle);

        /* After RX is added, add TX (watch notifies phone) */
        if (memcmp(gl_profile.char_uuid.uuid.uuid128, nus_rx_char_uuid, 16) == 0)
        {
            gl_profile.char_uuid.len = ESP_UUID_LEN_128;
            memcpy(gl_profile.char_uuid.uuid.uuid128, nus_tx_char_uuid, 16);
            esp_ble_gatts_add_char(gl_profile.service_handle,
                                   &gl_profile.char_uuid,
                                   ESP_GATT_PERM_READ,
                                   ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                   NULL, NULL);
        }
        else if (memcmp(gl_profile.char_uuid.uuid.uuid128, nus_tx_char_uuid, 16) == 0)
        {
            tx_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "TX char handle = %d", tx_char_handle);

            /* Add CCCD descriptor for TX */
            esp_bt_uuid_t cccd_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
            };
            esp_ble_gatts_add_char_descr(gl_profile.service_handle,
                                         &cccd_uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         NULL, NULL);
        }
        break;

    /* ---------- Descriptor added ---------- */
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(GATTS_TAG, "ADD_DESCR attr_handle %d", param->add_char_descr.attr_handle);
        break;

    /* ---------- Read request ---------- */
    case ESP_GATTS_READ_EVT:
    {
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 0;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                    param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
    }

    /* ---------- Write request ---------- */
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {
            ESP_LOGI(GATTS_TAG, "WRITE len=%d", param->write.len);
            ESP_LOG_BUFFER_HEX(GATTS_TAG, param->write.value, param->write.len);

            /* Parse TIME:<epoch>:<tz_offset_min> from phone */
            if (param->write.len > 5 && memcmp(param->write.value, "TIME:", 5) == 0)
            {
                char buf[64];
                int copy_len = param->write.len < (sizeof(buf) - 1) ? param->write.len : (sizeof(buf) - 1);
                memcpy(buf, param->write.value, copy_len);
                buf[copy_len] = '\0';

                /* Format: TIME:epoch:tz_offset_minutes  e.g. TIME:1709251200:330 */
                char *epoch_str = buf + 5;
                char *tz_str = strchr(epoch_str, ':');
                int tz_offset_sec = 0;
                if (tz_str)
                {
                    *tz_str = '\0';
                    tz_str++;
                    tz_offset_sec = atoi(tz_str) * 60;
                }

                long epoch = atol(epoch_str);
                if (epoch > 1000000000L)
                {
                    struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
                    settimeofday(&tv, NULL);

                    /* Set timezone as a fixed UTC offset */
                    char tz_env[32];
                    int abs_off = tz_offset_sec < 0 ? -tz_offset_sec : tz_offset_sec;
                    int tz_h = abs_off / 3600;
                    int tz_m = (abs_off % 3600) / 60;
                    /* POSIX TZ sign is inverted: UTC+5:30 → "UTC-5:30" */
                    snprintf(tz_env, sizeof(tz_env), "UTC%c%d:%02d",
                             tz_offset_sec >= 0 ? '-' : '+', tz_h, tz_m);
                    setenv("TZ", tz_env, 1);
                    tzset();

                    ESP_LOGI(GATTS_TAG, "Time set: epoch=%ld  TZ=%s", epoch, tz_env);
                }
            }
        }
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                    param->write.trans_id, ESP_GATT_OK, NULL);
        break;

    /* ---------- MTU negotiated ---------- */
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "MTU %d", param->mtu.mtu);
        break;

    /* ---------- Client connected (accept link immediately, prompt user) ---------- */
    case ESP_GATTS_CONNECT_EVT:
    {
        ESP_LOGI(GATTS_TAG, "CONNECTED conn_id=%d  %02x:%02x:%02x:%02x:%02x:%02x",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1],
                 param->connect.remote_bda[2], param->connect.remote_bda[3],
                 param->connect.remote_bda[4], param->connect.remote_bda[5]);

        if (!ble_enabled)
        {
            /* BLE is toggled off – reject immediately */
            esp_ble_gatts_close(gatts_if, param->connect.conn_id);
            break;
        }

        /* Accept the BLE link immediately so the phone stays connected */
        esp_ble_conn_update_params_t cp = {0};
        memcpy(cp.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        cp.min_int = 0x10; /* 20 ms */
        cp.max_int = 0x20; /* 40 ms */
        cp.latency = 0;
        cp.timeout = 400; /* 4 s   */
        esp_ble_gap_update_conn_params(&cp);

        is_connected = true;
        conn_id = param->connect.conn_id;
        gatts_if_handle = gatts_if;

        /* Gate app-level communication until user approves */
        user_approved = false;
        pending_approval = true;
        memcpy(pending_bda, param->connect.remote_bda, 6);
        break;
    }

    /* ---------- Client disconnected ---------- */
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "DISCONNECTED reason 0x%x", param->disconnect.reason);
        is_connected = false;
        pending_approval = false;
        user_approved = false;
        /* Restart advertising only if BLE is enabled */
        if (ble_enabled)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK)
        {
            ESP_LOGW(GATTS_TAG, "CONF_EVT status %d", param->conf.status);
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 *  Top-level GATTS dispatcher
 * ================================================================ */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile.gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGE(GATTS_TAG, "Reg app failed, status %d", param->reg.status);
            return;
        }
    }

    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile.gatts_if)
    {
        if (gl_profile.gatts_cb)
        {
            gl_profile.gatts_cb(event, gatts_if, param);
        }
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */
void ble_server_init(void)
{
    esp_err_t ret;

    /* Release Classic-BT memory – we only need BLE */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "controller init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(0);
    esp_ble_gatt_set_local_mtu(247); /* smaller MTU = less buffer memory */

    /* ---- Security / bonding configuration ---- */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND; /* bond so reconnects work    */
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;       /* "Just Works" – no passkey  */
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t passkey = 0; /* unused in Just Works       */
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(auth_option));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(oob_support));

    ESP_LOGI(GATTS_TAG, "BLE Server init complete (security configured)");
}

void ble_server_send_alert(const char *message)
{
    ESP_LOGI(GATTS_TAG, "send_alert: conn=%d approved=%d if=%d tx_h=%d",
             is_connected, user_approved, gatts_if_handle, tx_char_handle);

    if (is_connected && user_approved && gatts_if_handle != ESP_GATT_IF_NONE && tx_char_handle != 0)
    {
        uint16_t len = (uint16_t)strlen(message);
        esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if_handle, conn_id, tx_char_handle,
                                                    len, (uint8_t *)message, false);
        ESP_LOGI(GATTS_TAG, "Notify: '%s' len=%d handle=%d ret=%s",
                 message, len, tx_char_handle, esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGW(GATTS_TAG, "Cannot send: conn=%d approved=%d if=%d handle=%d",
                 is_connected, user_approved, (int)gatts_if_handle, tx_char_handle);
    }
}

bool ble_server_is_connected(void)
{
    return is_connected && user_approved;
}

/* ================================================================
 *  Connection-approval API
 * ================================================================ */
bool ble_server_has_pending_connection(void)
{
    return pending_approval;
}

void ble_server_get_pending_bda(uint8_t bda[6])
{
    memcpy(bda, pending_bda, 6);
}

void ble_server_accept_connection(void)
{
    if (!pending_approval)
    {
        return;
    }
    pending_approval = false;
    user_approved = true;
    ESP_LOGI(GATTS_TAG, "Connection ACCEPTED by user");
}

void ble_server_reject_connection(void)
{
    if (!pending_approval)
    {
        return;
    }
    pending_approval = false;
    user_approved = false;
    /* Disconnect the phone */
    if (is_connected)
    {
        esp_ble_gatts_close(gatts_if_handle, conn_id);
        is_connected = false;
    }
    ESP_LOGI(GATTS_TAG, "Connection REJECTED by user");
}

/* ================================================================
 *  Enable / Disable API
 * ================================================================ */
void ble_server_enable(void)
{
    if (ble_enabled)
    {
        return;
    }
    ble_enabled = true;
    esp_ble_gap_start_advertising(&adv_params);
    ESP_LOGI(GATTS_TAG, "BLE enabled – advertising");
}

void ble_server_disable(void)
{
    if (!ble_enabled)
    {
        return;
    }
    ble_enabled = false;

    if (pending_approval)
    {
        pending_approval = false;
        user_approved = false;
    }
    if (is_connected)
    {
        esp_ble_gatts_close(gatts_if_handle, conn_id);
        is_connected = false;
        user_approved = false;
    }
    esp_ble_gap_stop_advertising();
    ESP_LOGI(GATTS_TAG, "BLE disabled – advertising stopped");
}

bool ble_server_is_enabled(void)
{
    return ble_enabled;
}
