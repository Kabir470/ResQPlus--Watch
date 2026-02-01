#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

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
            wifi_set_status("WiFi: disconnected");
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

static void wifi_init_once(void)
{
    if (s_inited)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());

    // Ignore error if already created by something else.
    (void)esp_event_loop_create_default();

    (void)esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    s_inited = true;
}

void wifi_manager_connect(void)
{
    wifi_manager_connect_with_credentials(NULL, NULL);
}

void wifi_manager_connect_with_credentials(const char *ssid, const char *password)
{
    wifi_init_once();

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
    strncpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password));

    // If password is empty, allow open networks.
    wifi_config.sta.threshold.authmode = (s_password[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start is idempotent; connect will (re)attempt.
    (void)esp_wifi_start();
    (void)esp_wifi_connect();

    ESP_LOGI(TAG, "WiFi connect requested (SSID=%s)", s_ssid);
    wifi_set_status("WiFi: connecting");
}
