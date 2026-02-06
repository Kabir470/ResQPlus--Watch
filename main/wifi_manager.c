#include "wifi_manager.h"

#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "esp_err.h"

#include "freertos/portmacro.h"

static const char *TAG = "wifi";

static bool s_inited;
static char s_status_text[32] = "WiFi: off";

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;

static char s_ssid[33];
static char s_password[65];

const char *wifi_manager_get_status_text(void)
{
    return s_status_text;
}

void wifi_manager_get_status_text_copy(char *out, unsigned out_size)
{
    if (!out || out_size == 0)
    {
        return;
    }

    portENTER_CRITICAL(&s_status_lock);
    strncpy(out, s_status_text, out_size);
    portEXIT_CRITICAL(&s_status_lock);
    out[out_size - 1] = '\0';
}

static void wifi_set_status(const char *text)
{
    if (!text || text[0] == '\0')
    {
        return;
    }

    portENTER_CRITICAL(&s_status_lock);
    strncpy(s_status_text, text, sizeof(s_status_text));
    s_status_text[sizeof(s_status_text) - 1] = '\0';
    portEXIT_CRITICAL(&s_status_lock);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            wifi_set_status("WiFi: connecting");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            const wifi_event_sta_disconnected_t *disc = (const wifi_event_sta_disconnected_t *)event_data;
            if (disc)
            {
                char buf[32];
                // Keep short; fits the UI label buffer.
                snprintf(buf, sizeof(buf), "WiFi: disc %d", (int)disc->reason);
                wifi_set_status(buf);
            }
            else
            {
                wifi_set_status("WiFi: disconnected");
            }
            // Keep it simple: auto-retry.
            (void)esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED)
        {
            wifi_set_status("WiFi: connected");
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            wifi_set_status("WiFi: got IP");
        }
    }
}

static esp_err_t wifi_init_once(void)
{
    if (s_inited)
    {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: netif err");
        return err;
    }

    // Ignore error if already created by something else.
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "event loop create failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: evloop err");
        return err;
    }

    if (esp_netif_create_default_wifi_sta() == NULL)
    {
        ESP_LOGE(TAG, "create_default_wifi_sta failed");
        wifi_set_status("WiFi: sta err");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        // Commonly ESP_ERR_NO_MEM when internal RAM is tight.
        wifi_set_status((err == ESP_ERR_NO_MEM) ? "WiFi: no mem" : "WiFi: init err");
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi event handler register failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: evt err");
        return err;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ip event handler register failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: ip evt err");
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: mode err");
        return err;
    }

    // Avoid power-save edge cases while bringing up networking.
    (void)esp_wifi_set_ps(WIFI_PS_NONE);

    s_inited = true;

    wifi_set_status("WiFi: ready");
    return ESP_OK;
}

void wifi_manager_connect(void)
{
    wifi_manager_connect_with_credentials(NULL, NULL);
}

void wifi_manager_connect_with_credentials(const char *ssid, const char *password)
{
    esp_err_t err = wifi_init_once();
    if (err != ESP_OK)
    {
        // Status already set.
        return;
    }

    const char *use_ssid = ssid;
    const char *use_password = password;

    if (!use_ssid || use_ssid[0] == '\0')
    {
#if defined(CONFIG_MAIN_WIFI_SSID) && defined(CONFIG_MAIN_WIFI_PASSWORD)
        use_ssid = CONFIG_MAIN_WIFI_SSID;
        use_password = CONFIG_MAIN_WIFI_PASSWORD;
#else
        ESP_LOGW(TAG, "WiFi credentials not set. Enter SSID/PW on the WiFi screen (or set menuconfig: Main -> WiFi)");
        wifi_set_status("WiFi: enter SSID");
        return;
#endif
    }

    if (!use_password)
    {
        use_password = "";
    }

    strncpy(s_ssid, use_ssid, sizeof(s_ssid));
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_password, use_password, sizeof(s_password));
    s_password[sizeof(s_password) - 1] = '\0';

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    strncpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    // If password is empty, allow open networks. Otherwise allow WPA2/WPA3.
    wifi_config.sta.threshold.authmode = (s_password[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_WPA3_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        wifi_set_status("WiFi: cfg err");
        return;
    }

    // Start is idempotent; connect will (re)attempt.
    (void)esp_wifi_start();
    (void)esp_wifi_connect();

    ESP_LOGI(TAG, "WiFi connect requested (SSID=%s)", s_ssid);
    wifi_set_status("WiFi: connecting");
}
