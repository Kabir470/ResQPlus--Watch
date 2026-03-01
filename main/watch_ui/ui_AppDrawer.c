/*
 * App Drawer Screen – slides up from the main watch face.
 * Shows a grid of app icons (like a smartwatch app menu).
 * Swipe down or swipe right to go back to the watch face.
 */

#include "ui.h"
#include <sys/time.h>
#include <time.h>

lv_obj_t *ui_AppDrawer = NULL;

/* ── Forward declarations ──────────────────────────────────────────────── */
static void app_drawer_gesture_cb(lv_event_t *e);
static void app_clock_btn_cb(lv_event_t *e);
static void app_settings_btn_cb(lv_event_t *e);
static void app_about_btn_cb(lv_event_t *e);

/* ── Helper: create a round icon button with label ─────────────────────── */
static lv_obj_t *create_app_icon(lv_obj_t *parent, const char *symbol,
                                 const char *label_text, lv_color_t bg_color,
                                 int x, int y, lv_event_cb_t cb)
{
    /* Icon circle */
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 70, 70);
    lv_obj_set_x(btn, x);
    lv_obj_set_y(btn, y);
    lv_obj_set_align(btn, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_bg_opa(btn, 255, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    /* Symbol inside button */
    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_center(icon);

    /* Label below */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_x(lbl, x);
    lv_obj_set_y(lbl, y + 48);
    lv_obj_set_align(lbl, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);

    if (cb)
    {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

/* ── Enable gesture bubbling so swipes work over children ────────────── */
static void enable_gesture_bubble_recursive_ad(lv_obj_t *parent)
{
    uint32_t cnt = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
        enable_gesture_bubble_recursive_ad(child);
    }
}

/* ── Screen init ───────────────────────────────────────────────────────── */
void ui_AppDrawer_screen_init(void)
{
    ui_AppDrawer = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_AppDrawer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_AppDrawer, lv_color_hex(0x0A0A0A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AppDrawer, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Title */
    lv_obj_t *title = lv_label_create(ui_AppDrawer);
    lv_label_set_text(title, "Apps");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_x(title, 0);
    lv_obj_set_y(title, -200);
    lv_obj_set_align(title, LV_ALIGN_CENTER);

    /* Swipe hint at bottom */
    lv_obj_t *hint = lv_label_create(ui_AppDrawer);
    lv_label_set_text(hint, LV_SYMBOL_DOWN "  swipe down  " LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_x(hint, 0);
    lv_obj_set_y(hint, 215);
    lv_obj_set_align(hint, LV_ALIGN_CENTER);

    /* ── App Icons Grid ── */
    /* Row 1 */
    create_app_icon(ui_AppDrawer, LV_SYMBOL_SETTINGS, "Settings",
                    lv_color_hex(0x444444), -70, -100, app_settings_btn_cb);

    create_app_icon(ui_AppDrawer, LV_SYMBOL_BELL, "Clock",
                    lv_color_hex(0x0055AA), 70, -100, app_clock_btn_cb);

    /* Row 2 */
    create_app_icon(ui_AppDrawer, LV_SYMBOL_LIST, "About",
                    lv_color_hex(0x226666), -70, 20, app_about_btn_cb);

    create_app_icon(ui_AppDrawer, LV_SYMBOL_BATTERY_FULL, "Battery",
                    lv_color_hex(0x664400), 70, 20, NULL);

    /* Enable gesture bubble on all children */
    enable_gesture_bubble_recursive_ad(ui_AppDrawer);

    /* Swipe gesture → back to watch face */
    lv_obj_add_event_cb(ui_AppDrawer, app_drawer_gesture_cb, LV_EVENT_GESTURE, NULL);
}

void ui_AppDrawer_screen_destroy(void)
{
    if (ui_AppDrawer)
        lv_obj_del(ui_AppDrawer);
    ui_AppDrawer = NULL;
}

/* ── Gesture: swipe down or right → back to watch face ─────────────────── */
static void app_drawer_gesture_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE)
        return;

    static uint32_t last_ms;
    uint32_t now = lv_tick_get();
    if (now - last_ms < 400)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
        return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_BOTTOM)
    {
        last_ms = now;
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0,
                          &ui_Screen1_screen_init);
    }
    else if (dir == LV_DIR_RIGHT)
    {
        last_ms = now;
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                          &ui_Screen1_screen_init);
    }
}

/* ── Button callbacks ─────────────────────────────────────────────────── */
static void app_clock_btn_cb(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_TimeSettings, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0,
                      &ui_TimeSettings_screen_init);
}

static void app_settings_btn_cb(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_SettingsScreen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0,
                      &ui_SettingsScreen_screen_init);
}

static void app_about_btn_cb(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_AboutScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0,
                      &ui_AboutScreen_screen_init);
}
