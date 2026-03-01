/*
 * Time & Date Settings Screen
 * Allows the user to manually set hours, minutes, day, month, and year
 * using rollers — similar to a phone clock settings.
 */

#include "ui.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "time_settings";

lv_obj_t *ui_TimeSettings = NULL;

/* ── Roller widgets ────────────────────────────────────────────────────── */
static lv_obj_t *roller_hour = NULL;
static lv_obj_t *roller_min = NULL;
static lv_obj_t *roller_day = NULL;
static lv_obj_t *roller_month = NULL;
static lv_obj_t *roller_year = NULL;

/* ── Forward declarations ──────────────────────────────────────────────── */
static void save_btn_cb(lv_event_t *e);
static void back_btn_cb(lv_event_t *e);
static void gesture_cb_ts(lv_event_t *e);

/* ── Enable gesture bubbling ───────────────────────────────────────────── */
static void bubble_gestures_ts(lv_obj_t *parent)
{
    uint32_t cnt = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
        bubble_gestures_ts(child);
    }
}

/* ── Build roller option strings ───────────────────────────────────────── */
static char hour_opts[24 * 4]; /* "00\n01\n...\n23" */
static char min_opts[60 * 4];  /* "00\n01\n...\n59" */
static char day_opts[31 * 4];  /* "01\n02\n...\n31" */
static char year_opts[20 * 6]; /* "2024\n2025\n...\n2040" */

static const char *month_opts =
    "Jan\nFeb\nMar\nApr\nMay\nJun\n"
    "Jul\nAug\nSep\nOct\nNov\nDec";

static bool opts_built = false;

static void build_roller_options(void)
{
    if (opts_built)
        return;

    /* Hours 00..23 */
    hour_opts[0] = '\0';
    for (int i = 0; i < 24; i++)
    {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%s%02d", i ? "\n" : "", i);
        strcat(hour_opts, tmp);
    }

    /* Minutes 00..59 */
    min_opts[0] = '\0';
    for (int i = 0; i < 60; i++)
    {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%s%02d", i ? "\n" : "", i);
        strcat(min_opts, tmp);
    }

    /* Days 01..31 */
    day_opts[0] = '\0';
    for (int i = 1; i <= 31; i++)
    {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%s%02d", i > 1 ? "\n" : "", i);
        strcat(day_opts, tmp);
    }

    /* Years 2024..2040 */
    year_opts[0] = '\0';
    for (int i = 2024; i <= 2040; i++)
    {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%s%d", i > 2024 ? "\n" : "", i);
        strcat(year_opts, tmp);
    }

    opts_built = true;
}

/* ── Style a roller for the dark watch theme ───────────────────────────── */
static void style_roller(lv_obj_t *r, int w)
{
    lv_obj_set_width(r, w);
    lv_obj_set_height(r, 130);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(r, 255, LV_PART_MAIN);
    lv_obj_set_style_text_color(r, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(r, 12, LV_PART_MAIN);

    /* Selected row */
    lv_obj_set_style_bg_color(r, lv_color_hex(0x0055AA), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(r, 255, LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_24, LV_PART_SELECTED);
}

/* ── Screen init ───────────────────────────────────────────────────────── */
void ui_TimeSettings_screen_init(void)
{
    build_roller_options();

    ui_TimeSettings = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_TimeSettings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_TimeSettings, lv_color_hex(0x050510), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_TimeSettings, 255, LV_PART_MAIN);

    /* ── Title ─────────────────────────────────────────────────────────── */
    lv_obj_t *title = lv_label_create(ui_TimeSettings);
    lv_label_set_text(title, LV_SYMBOL_BELL "  Set Time");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_x(title, 0);
    lv_obj_set_y(title, -205);
    lv_obj_set_align(title, LV_ALIGN_CENTER);

    /* ── "Time" sub-label ──────────────────────────────────────────────── */
    lv_obj_t *time_lbl = lv_label_create(ui_TimeSettings);
    lv_label_set_text(time_lbl, "TIME");
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0x6688CC), 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_x(time_lbl, -55);
    lv_obj_set_y(time_lbl, -165);
    lv_obj_set_align(time_lbl, LV_ALIGN_CENTER);

    /* ── Hour roller ───────────────────────────────────────────────────── */
    roller_hour = lv_roller_create(ui_TimeSettings);
    lv_roller_set_options(roller_hour, hour_opts, LV_ROLLER_MODE_NORMAL);
    style_roller(roller_hour, 65);
    lv_obj_set_x(roller_hour, -80);
    lv_obj_set_y(roller_hour, -95);
    lv_obj_set_align(roller_hour, LV_ALIGN_CENTER);

    /* ":" separator */
    lv_obj_t *colon = lv_label_create(ui_TimeSettings);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
    lv_obj_set_x(colon, -40);
    lv_obj_set_y(colon, -95);
    lv_obj_set_align(colon, LV_ALIGN_CENTER);

    /* ── Minute roller ─────────────────────────────────────────────────── */
    roller_min = lv_roller_create(ui_TimeSettings);
    lv_roller_set_options(roller_min, min_opts, LV_ROLLER_MODE_NORMAL);
    style_roller(roller_min, 65);
    lv_obj_set_x(roller_min, 0);
    lv_obj_set_y(roller_min, -95);
    lv_obj_set_align(roller_min, LV_ALIGN_CENTER);

    /* ── "Date" sub-label ──────────────────────────────────────────────── */
    lv_obj_t *date_lbl = lv_label_create(ui_TimeSettings);
    lv_label_set_text(date_lbl, "DATE");
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(0x6688CC), 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_x(date_lbl, 0);
    lv_obj_set_y(date_lbl, -15);
    lv_obj_set_align(date_lbl, LV_ALIGN_CENTER);

    /* ── Day roller ────────────────────────────────────────────────────── */
    roller_day = lv_roller_create(ui_TimeSettings);
    lv_roller_set_options(roller_day, day_opts, LV_ROLLER_MODE_NORMAL);
    style_roller(roller_day, 55);
    lv_obj_set_x(roller_day, -90);
    lv_obj_set_y(roller_day, 55);
    lv_obj_set_align(roller_day, LV_ALIGN_CENTER);

    /* ── Month roller ──────────────────────────────────────────────────── */
    roller_month = lv_roller_create(ui_TimeSettings);
    lv_roller_set_options(roller_month, month_opts, LV_ROLLER_MODE_NORMAL);
    style_roller(roller_month, 65);
    lv_obj_set_x(roller_month, -20);
    lv_obj_set_y(roller_month, 55);
    lv_obj_set_align(roller_month, LV_ALIGN_CENTER);

    /* ── Year roller ───────────────────────────────────────────────────── */
    roller_year = lv_roller_create(ui_TimeSettings);
    lv_roller_set_options(roller_year, year_opts, LV_ROLLER_MODE_NORMAL);
    style_roller(roller_year, 70);
    lv_obj_set_x(roller_year, 60);
    lv_obj_set_y(roller_year, 55);
    lv_obj_set_align(roller_year, LV_ALIGN_CENTER);

    /* ── Pre-fill rollers with current time ────────────────────────────── */
    time_t now;
    time(&now);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year + 1900 >= 2024)
    {
        lv_roller_set_selected(roller_hour, ti.tm_hour, LV_ANIM_OFF);
        lv_roller_set_selected(roller_min, ti.tm_min, LV_ANIM_OFF);
        lv_roller_set_selected(roller_day, ti.tm_mday - 1, LV_ANIM_OFF);
        lv_roller_set_selected(roller_month, ti.tm_mon, LV_ANIM_OFF);
        int yr_idx = ti.tm_year + 1900 - 2024;
        if (yr_idx < 0)
            yr_idx = 0;
        if (yr_idx > 16)
            yr_idx = 16;
        lv_roller_set_selected(roller_year, yr_idx, LV_ANIM_OFF);
    }
    else
    {
        /* Default: 12:00, 01 Jan 2026 */
        lv_roller_set_selected(roller_hour, 12, LV_ANIM_OFF);
        lv_roller_set_selected(roller_min, 0, LV_ANIM_OFF);
        lv_roller_set_selected(roller_day, 0, LV_ANIM_OFF);
        lv_roller_set_selected(roller_month, 0, LV_ANIM_OFF);
        lv_roller_set_selected(roller_year, 2, LV_ANIM_OFF); /* 2026 */
    }

    /* ── Save button ───────────────────────────────────────────────────── */
    lv_obj_t *save_btn = lv_button_create(ui_TimeSettings);
    lv_obj_set_size(save_btn, 110, 45);
    lv_obj_set_x(save_btn, -55);
    lv_obj_set_y(save_btn, 160);
    lv_obj_set_align(save_btn, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x00AA44), 0);
    lv_obj_set_style_radius(save_btn, 22, 0);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(save_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(save_lbl);

    /* ── Cancel button ─────────────────────────────────────────────────── */
    lv_obj_t *cancel_btn = lv_button_create(ui_TimeSettings);
    lv_obj_set_size(cancel_btn, 100, 45);
    lv_obj_set_x(cancel_btn, 60);
    lv_obj_set_y(cancel_btn, 160);
    lv_obj_set_align(cancel_btn, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xCC2222), 0);
    lv_obj_set_style_radius(cancel_btn, 22, 0);
    lv_obj_add_event_cb(cancel_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, LV_SYMBOL_CLOSE " Back");
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(cancel_lbl);

    /* ── Gesture: swipe right → back to app drawer ─────────────────────── */
    bubble_gestures_ts(ui_TimeSettings);
    lv_obj_add_event_cb(ui_TimeSettings, gesture_cb_ts, LV_EVENT_GESTURE, NULL);
}

void ui_TimeSettings_screen_destroy(void)
{
    if (ui_TimeSettings)
        lv_obj_del(ui_TimeSettings);
    ui_TimeSettings = NULL;
    roller_hour = NULL;
    roller_min = NULL;
    roller_day = NULL;
    roller_month = NULL;
    roller_year = NULL;
}

/* ── Save: read rollers and set system time ────────────────────────────── */
static void save_btn_cb(lv_event_t *e)
{
    (void)e;

    int hour = lv_roller_get_selected(roller_hour);
    int minute = lv_roller_get_selected(roller_min);
    int day = lv_roller_get_selected(roller_day) + 1; /* roller is 0-based, day is 1-based */
    int month = lv_roller_get_selected(roller_month); /* 0..11 */
    int year = 2024 + lv_roller_get_selected(roller_year);

    struct tm ti = {0};
    ti.tm_hour = hour;
    ti.tm_min = minute;
    ti.tm_sec = 0;
    ti.tm_mday = day;
    ti.tm_mon = month;
    ti.tm_year = year - 1900;
    ti.tm_isdst = -1;

    time_t epoch = mktime(&ti);
    if (epoch < 0)
    {
        ESP_LOGW(TAG, "Invalid date/time selected");
        return;
    }

    struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "Time manually set: %04d-%02d-%02d %02d:%02d:00 (epoch=%ld)",
             year, month + 1, day, hour, minute, (long)epoch);

    /* Go back to app drawer */
    _ui_screen_change(&ui_AppDrawer, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0,
                      &ui_AppDrawer_screen_init);
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_AppDrawer, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0,
                      &ui_AppDrawer_screen_init);
}

/* ── Gesture: swipe right → back to app drawer ─────────────────────────── */
static void gesture_cb_ts(lv_event_t *e)
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
        _ui_screen_change(&ui_AppDrawer, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                          &ui_AppDrawer_screen_init);
    }
}
