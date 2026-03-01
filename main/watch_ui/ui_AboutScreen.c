/*
 * About / Device Info Screen
 * Shows device information: chip, firmware, battery, RAM, BLE, uptime.
 * Swipe right → back to Settings.
 */

#include "ui.h"
#include "ble_server.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include <stdio.h>

lv_obj_t *ui_AboutScreen = NULL;

/* ── Forward declarations ──────────────────────────────────────────────── */
static void gesture_cb_about(lv_event_t *e);

static void bubble_gestures_about(lv_obj_t *parent)
{
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; i++)
    {
        lv_obj_t *c = lv_obj_get_child(parent, i);
        lv_obj_add_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
        bubble_gestures_about(c);
    }
}

/* Helper: add an info row (label: value) */
static void info_row(lv_obj_t *parent, const char *label, const char *value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(row, 255, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x8899BB), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
}

/* ── Screen init ───────────────────────────────────────────────────────── */
void ui_AboutScreen_screen_init(void)
{
    ui_AboutScreen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_AboutScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_AboutScreen, lv_color_hex(0x050510), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AboutScreen, 255, LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(ui_AboutScreen);
    lv_label_set_text(title, LV_SYMBOL_LIST "  About");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(ui_AboutScreen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " swipe right to go back");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* Scrollable content */
    lv_obj_t *content = lv_obj_create(ui_AboutScreen);
    lv_obj_set_size(content, 380, 420);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    /* ── Gather device info ────────────────────────────────────────────── */
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    char cores_buf[8];
    lv_snprintf(cores_buf, sizeof(cores_buf), "%d", chip.cores);

    /* Free heap */
    char heap_buf[24];
    lv_snprintf(heap_buf, sizeof(heap_buf), "%lu KB",
                (unsigned long)(esp_get_free_heap_size() / 1024));

    /* Free internal RAM */
    char iram_buf[24];
    lv_snprintf(iram_buf, sizeof(iram_buf), "%lu KB",
                (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));

    /* Free PSRAM */
    char psram_buf[24];
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_free > 0)
        lv_snprintf(psram_buf, sizeof(psram_buf), "%lu KB",
                    (unsigned long)(psram_free / 1024));
    else
        lv_snprintf(psram_buf, sizeof(psram_buf), "N/A");

    /* BLE status */
    const char *ble_str;
    if (!ble_server_is_enabled())
        ble_str = "Off";
    else if (ble_server_is_connected())
        ble_str = "Connected";
    else
        ble_str = "On (idle)";

    /* Uptime */
    uint32_t up_sec = lv_tick_get() / 1000;
    char uptime_buf[24];
    lv_snprintf(uptime_buf, sizeof(uptime_buf), "%02lu:%02lu:%02lu",
                (unsigned long)(up_sec / 3600),
                (unsigned long)((up_sec % 3600) / 60),
                (unsigned long)(up_sec % 60));

    /* IDF version */
    const char *idf_ver = esp_get_idf_version();

    /* ── Populate rows ─────────────────────────────────────────────────── */
    info_row(content, "Device", "ESP32-S3");
    info_row(content, "Cores", cores_buf);
    info_row(content, "Display", "410x502 AMOLED");
    info_row(content, "Firmware", "ResQ+ v1.0");
    info_row(content, "IDF", idf_ver);
    info_row(content, "Free Heap", heap_buf);
    info_row(content, "Int. RAM", iram_buf);
    info_row(content, "PSRAM", psram_buf);
    info_row(content, "Bluetooth", ble_str);
    info_row(content, "Uptime", uptime_buf);

    /* ── Enable gesture bubble + swipe right ───────────────────────────── */
    bubble_gestures_about(ui_AboutScreen);
    lv_obj_add_event_cb(ui_AboutScreen, gesture_cb_about, LV_EVENT_GESTURE, NULL);
}

void ui_AboutScreen_screen_destroy(void)
{
    if (ui_AboutScreen)
        lv_obj_del(ui_AboutScreen);
    ui_AboutScreen = NULL;
}

/* ── Gesture: swipe right → back to Settings ───────────────────────────── */
static void gesture_cb_about(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE)
        return;

    static uint32_t last;
    uint32_t now = lv_tick_get();
    if (now - last < 400)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
        return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_RIGHT)
    {
        last = now;
        _ui_screen_change(&ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                          &ui_SettingsScreen_screen_init);
    }
}
