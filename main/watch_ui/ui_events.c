// Event handlers for SquareLine UI, implemented for ESP-IDF

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"

#include "bsp/display.h"
#include "lvgl.h"

#include "ui.h"

#include "wifi_manager.h"

static const char *TAG = "watch_ui";

static int s_alert_count = 0;

void startRescueSequence(lv_event_t *e)
{
    (void)e;

    s_alert_count++;

    if (ui_lblAlertCount)
    {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d", s_alert_count);
        lv_label_set_text(ui_lblAlertCount, buf);
    }

    ESP_LOGI(TAG, "Alert pressed, count=%d", s_alert_count);
}

void turnScreenOff(lv_event_t *e)
{
    (void)e;
    (void)bsp_display_backlight_off();
}

void setBrightness(lv_event_t *e)
{
    lv_obj_t *arc = lv_event_get_target(e);
    int value = lv_arc_get_value(arc);

    // UI arc range is 50..255; convert to percent.
    int percent = (value * 100) / 255;
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    ESP_LOGI(TAG, "Brightness arc=%d => %d%%", value, percent);
    (void)bsp_display_brightness_set(percent);
}

void connectWiFi(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "WiFi button pressed");
    wifi_manager_connect();
}
