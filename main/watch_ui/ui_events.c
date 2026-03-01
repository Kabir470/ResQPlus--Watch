// Event handlers for SquareLine UI, implemented for ESP-IDF

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"

#include "bsp/display.h"
#include "lvgl.h"

#include "ui.h"
#include "ble_server.h"

static const char *TAG = "watch_ui";

static int s_alert_count = 0;

/* ── Screen-off / wake state ───────────────────────────────────────────── */
static bool s_screen_is_off = false;
static uint32_t s_screen_off_tick = 0;

bool isScreenOff(void)
{
    return s_screen_is_off;
}

void setScreenOff(bool off)
{
    s_screen_is_off = off;
    if (off)
    {
        s_screen_off_tick = lv_tick_get();
        (void)bsp_display_backlight_off();
    }
}

bool canWakeScreen(void)
{
    if (!s_screen_is_off)
        return false;
    return (lv_tick_get() - s_screen_off_tick) > 2000; /* 2 s lockout */
}

void wakeScreen(int brightness_pct)
{
    if (brightness_pct < 5)
        brightness_pct = 40; /* sensible default */
    s_screen_is_off = false;
    (void)bsp_display_brightness_set(brightness_pct);
}

/* ── Alert handling ────────────────────────────────────────────────────── */

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

    /* Send BLE notification to the phone app */
    if (ble_server_is_connected())
    {
        char msg[32];
        lv_snprintf(msg, sizeof(msg), "Alert:%d", s_alert_count);
        ble_server_send_alert(msg);
        ESP_LOGI(TAG, "Alert sent via BLE, count=%d", s_alert_count);
    }
    else
    {
        ESP_LOGW(TAG, "Alert pressed but BLE not connected, count=%d", s_alert_count);
    }
}

void resetAlertCount(void)
{
    s_alert_count = 0;

    if (ui_lblAlertCount)
    {
        lv_label_set_text(ui_lblAlertCount, "0");
    }

    /* Notify phone app about reset */
    if (ble_server_is_connected())
    {
        ble_server_send_alert("Alert:0");
        ESP_LOGI(TAG, "Alert count reset, notified phone");
    }
    else
    {
        ESP_LOGI(TAG, "Alert count reset (BLE not connected)");
    }
}

/* ── Display helpers ───────────────────────────────────────────────────── */

void turnScreenOff(lv_event_t *e)
{
    (void)e;
    setScreenOff(true);
}

void toggleBluetooth(lv_event_t *e)
{
    (void)e;

    if (ble_server_is_enabled())
    {
        ble_server_disable();
        if (ui_btnBLE)
            lv_obj_set_style_bg_color(ui_btnBLE, lv_color_hex(0x444444),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_lblBLE)
            lv_label_set_text(ui_lblBLE, "OFF");
        ESP_LOGI(TAG, "Bluetooth OFF");
    }
    else
    {
        ble_server_enable();
        if (ui_btnBLE)
            lv_obj_set_style_bg_color(ui_btnBLE, lv_color_hex(0x0066FF),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_lblBLE)
            lv_label_set_text(ui_lblBLE, "ON");
        ESP_LOGI(TAG, "Bluetooth ON");
    }
}
