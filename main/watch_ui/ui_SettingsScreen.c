/*
 * Settings Screen – Smartwatch-style scrollable settings menu.
 * Dark theme, flex-column list with icon rows.
 * Swipe right → back to watch face.
 */

#include "ui.h"
#include "ble_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "bsp/display.h"

static const char *TAG_SET = "settings_ui";

/* ── Public variables ──────────────────────────────────────────────────── */
lv_obj_t *ui_SettingsScreen = NULL;
lv_obj_t *ui_btnBLE = NULL;
lv_obj_t *ui_lblBLE = NULL;

/* ── Screen-timeout state ──────────────────────────────────────────────── */
static lv_obj_t *lbl_timeout_val = NULL;
static const int timeout_opts[] = {0, 10, 15, 30, 60};
static const char *timeout_strs[] = {"Off", "10 s", "15 s", "30 s", "60 s"};
#define NUM_TO (sizeof(timeout_opts) / sizeof(timeout_opts[0]))
static int timeout_idx = 3; /* default 30 s */

int getScreenTimeoutSeconds(void)
{
    return timeout_opts[timeout_idx];
}

/* ── Brightness slider ─────────────────────────────────────────────────── */
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_val_lbl = NULL;

/* ── Forward declarations ──────────────────────────────────────────────── */
static void gesture_cb(lv_event_t *e);
static void brightness_cb(lv_event_t *e);
static void ble_toggle_cb(lv_event_t *e);
static void timeout_cycle_cb(lv_event_t *e);
static void screen_off_cb(lv_event_t *e);
static void about_cb(lv_event_t *e);
static void restart_cb(lv_event_t *e);
static void restart_timer_cb(lv_timer_t *t);

/* ── Helpers ───────────────────────────────────────────────────────────── */
static void bubble_gestures(lv_obj_t *parent)
{
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; i++)
    {
        lv_obj_t *c = lv_obj_get_child(parent, i);
        lv_obj_add_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
        bubble_gestures(c);
    }
}

/* Create a section header label (e.g. "DISPLAY") */
static lv_obj_t *section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x5588CC), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(lbl, 8, 0);
    return lbl;
}

/* Create a row card */
static lv_obj_t *make_row(lv_obj_t *parent, int h)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), h);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(row, 255, 0);
    lv_obj_set_style_radius(row, 14, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_left(row, 14, 0);
    lv_obj_set_style_pad_right(row, 14, 0);
    lv_obj_set_style_pad_top(row, 0, 0);
    lv_obj_set_style_pad_bottom(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    return row;
}

/* Add icon + text to a row and return the row */
static void row_icon_text(lv_obj_t *row, const char *icon, lv_color_t icol,
                          const char *text)
{
    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, icol, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_22, 0);

    lv_obj_t *lb = lv_label_create(row);
    lv_label_set_text(lb, text);
    lv_obj_set_style_text_color(lb, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_18, 0);
    lv_obj_set_flex_grow(lb, 1);
}

/* ── Screen init ───────────────────────────────────────────────────────── */
void ui_SettingsScreen_screen_init(void)
{
    ui_SettingsScreen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_SettingsScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_SettingsScreen, lv_color_hex(0x050510),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_SettingsScreen, 255, LV_PART_MAIN);

    /* ── Title ─────────────────────────────────────────────────────────── */
    lv_obj_t *title = lv_label_create(ui_SettingsScreen);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* ── Scrollable list ───────────────────────────────────────────────── */
    lv_obj_t *list = lv_obj_create(ui_SettingsScreen);
    lv_obj_set_size(list, 380, 440);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_opa(list, 0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    /* ────────────────────── DISPLAY SECTION ───────────────────────────── */
    section_header(list, "DISPLAY");

    /* -- Brightness row (taller, with slider) -- */
    {
        lv_obj_t *card = lv_obj_create(list);
        lv_obj_set_size(card, lv_pct(100), 100);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
        lv_obj_set_style_bg_opa(card, 255, 0);
        lv_obj_set_style_radius(card, 14, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 8, 0);

        /* Top row: icon + label + value */
        lv_obj_t *top = lv_obj_create(card);
        lv_obj_set_size(top, lv_pct(100), 30);
        lv_obj_set_style_bg_opa(top, 0, 0);
        lv_obj_set_style_border_width(top, 0, 0);
        lv_obj_set_style_pad_all(top, 0, 0);
        lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(top, 10, 0);

        lv_obj_t *ic = lv_label_create(top);
        lv_label_set_text(ic, LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_color(ic, lv_color_hex(0xFFAA00), 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);

        lv_obj_t *lb = lv_label_create(top);
        lv_label_set_text(lb, "Brightness");
        lv_obj_set_style_text_color(lb, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_18, 0);
        lv_obj_set_flex_grow(lb, 1);

        brightness_val_lbl = lv_label_create(top);
        lv_label_set_text(brightness_val_lbl, "39%");
        lv_obj_set_style_text_color(brightness_val_lbl, lv_color_hex(0xAABBDD), 0);
        lv_obj_set_style_text_font(brightness_val_lbl, &lv_font_montserrat_16, 0);

        /* Slider */
        brightness_slider = lv_slider_create(card);
        lv_obj_set_width(brightness_slider, lv_pct(95));
        lv_obj_set_height(brightness_slider, 12);
        lv_slider_set_range(brightness_slider, 10, 255);
        lv_slider_set_value(brightness_slider, 100, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x333355),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xFFAA00),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xFFCC44),
                                  LV_PART_KNOB);
        lv_obj_set_style_pad_all(brightness_slider, 4, LV_PART_KNOB);
        lv_obj_add_event_cb(brightness_slider, brightness_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* ────────────────────── CONNECTIVITY SECTION ──────────────────────── */
    section_header(list, "CONNECTIVITY");

    /* -- Bluetooth row -- */
    {
        lv_obj_t *row = make_row(list, 58);
        row_icon_text(row, LV_SYMBOL_BLUETOOTH, lv_color_hex(0x4488FF),
                      "Bluetooth");

        /* Toggle pill */
        ui_btnBLE = lv_button_create(row);
        lv_obj_set_size(ui_btnBLE, 80, 36);
        lv_obj_set_style_radius(ui_btnBLE, 18, 0);
        lv_obj_set_style_shadow_width(ui_btnBLE, 0, 0);
        lv_obj_set_style_border_width(ui_btnBLE, 0, 0);
        bool ble_on = ble_server_is_enabled();
        lv_obj_set_style_bg_color(ui_btnBLE,
                                  lv_color_hex(ble_on ? 0x0066FF : 0x444444), 0);
        lv_obj_set_style_bg_opa(ui_btnBLE, 255, 0);
        lv_obj_remove_flag(ui_btnBLE, LV_OBJ_FLAG_SCROLLABLE);

        ui_lblBLE = lv_label_create(ui_btnBLE);
        lv_label_set_text(ui_lblBLE, ble_on ? "ON" : "OFF");
        lv_obj_set_style_text_color(ui_lblBLE, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ui_lblBLE, &lv_font_montserrat_16, 0);
        lv_obj_center(ui_lblBLE);

        lv_obj_add_event_cb(ui_btnBLE, ble_toggle_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ────────────────────── GENERAL SECTION ───────────────────────────── */
    section_header(list, "GENERAL");

    /* -- Screen Timeout row -- */
    {
        lv_obj_t *row = make_row(list, 58);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        row_icon_text(row, LV_SYMBOL_EYE_CLOSE, lv_color_hex(0xAA66DD),
                      "Timeout");

        lbl_timeout_val = lv_label_create(row);
        lv_label_set_text(lbl_timeout_val, timeout_strs[timeout_idx]);
        lv_obj_set_style_text_color(lbl_timeout_val, lv_color_hex(0xAABBDD), 0);
        lv_obj_set_style_text_font(lbl_timeout_val, &lv_font_montserrat_16, 0);

        lv_obj_add_event_cb(row, timeout_cycle_cb, LV_EVENT_CLICKED, NULL);
    }

    /* -- Screen Off row -- */
    {
        lv_obj_t *row = make_row(list, 58);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        row_icon_text(row, LV_SYMBOL_POWER, lv_color_hex(0xDD3333),
                      "Screen Off");

        lv_obj_t *arr = lv_label_create(row);
        lv_label_set_text(arr, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arr, lv_color_hex(0x666666), 0);

        lv_obj_add_event_cb(row, screen_off_cb, LV_EVENT_CLICKED, NULL);
    }

    /* -- About row -- */
    {
        lv_obj_t *row = make_row(list, 58);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        row_icon_text(row, LV_SYMBOL_LIST, lv_color_hex(0x44BBAA),
                      "About");

        lv_obj_t *arr = lv_label_create(row);
        lv_label_set_text(arr, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arr, lv_color_hex(0x666666), 0);

        lv_obj_add_event_cb(row, about_cb, LV_EVENT_CLICKED, NULL);
    }

    /* -- Restart row -- */
    {
        lv_obj_t *row = make_row(list, 58);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        row_icon_text(row, LV_SYMBOL_REFRESH, lv_color_hex(0xFF8800),
                      "Restart");

        lv_obj_t *arr = lv_label_create(row);
        lv_label_set_text(arr, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arr, lv_color_hex(0x666666), 0);

        lv_obj_add_event_cb(row, restart_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ── Gesture bubble + swipe-right ──────────────────────────────────── */
    bubble_gestures(ui_SettingsScreen);
    lv_obj_add_event_cb(ui_SettingsScreen, gesture_cb, LV_EVENT_GESTURE, NULL);
}

void ui_SettingsScreen_screen_destroy(void)
{
    if (ui_SettingsScreen)
        lv_obj_del(ui_SettingsScreen);

    ui_SettingsScreen = NULL;
    ui_btnBLE = NULL;
    ui_lblBLE = NULL;
    brightness_slider = NULL;
    brightness_val_lbl = NULL;
    lbl_timeout_val = NULL;
}

/* ── Gesture: swipe right → back to watch face ─────────────────────────── */
static void gesture_cb(lv_event_t *e)
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
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                          &ui_Screen1_screen_init);
    }
}

/* ── Brightness slider callback ────────────────────────────────────────── */
static void brightness_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int val = lv_slider_get_value(sl);
    int pct = (val * 100) / 255;
    if (pct < 1)
        pct = 1;
    if (pct > 100)
        pct = 100;

    ESP_LOGI(TAG_SET, "Brightness slider=%d => %d%%", val, pct);
    bsp_display_brightness_set(pct);

    if (brightness_val_lbl)
    {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(brightness_val_lbl, buf);
    }
}

/* ── BLE toggle callback ──────────────────────────────────────────────── */
static void ble_toggle_cb(lv_event_t *e)
{
    (void)e;
    toggleBluetooth(NULL); /* reuse existing event handler */
}

/* ── Screen timeout – tap to cycle ─────────────────────────────────────── */
static void timeout_cycle_cb(lv_event_t *e)
{
    (void)e;
    timeout_idx = (timeout_idx + 1) % NUM_TO;
    ESP_LOGI(TAG_SET, "Screen timeout = %s", timeout_strs[timeout_idx]);
    if (lbl_timeout_val)
        lv_label_set_text(lbl_timeout_val, timeout_strs[timeout_idx]);
}

/* ── Screen off callback ───────────────────────────────────────────────── */
static void screen_off_cb(lv_event_t *e)
{
    turnScreenOff(e);
}

/* ── About callback ────────────────────────────────────────────────────── */
static void about_cb(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_AboutScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0,
                      &ui_AboutScreen_screen_init);
}

/* ── Restart callback ──────────────────────────────────────────────────── */
static void restart_cb(lv_event_t *e)
{
    (void)e;

    /* Show "Restarting..." overlay */
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, 220, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(overlay, 0, 0);

    lv_obj_t *lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH " Restarting...");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    /* Delay restart by 1 s so user sees the message */
    lv_timer_create(restart_timer_cb, 1000, overlay);
}

static void restart_timer_cb(lv_timer_t *t)
{
    lv_obj_t *overlay = lv_timer_get_user_data(t);
    if (overlay)
        lv_obj_del(overlay);
    lv_timer_delete(t);
    esp_restart();
}
