// WiFi login screen (ported from wifi_login_demo attachment)

#include "ui.h"

#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

lv_obj_t *ui_WiFiLoginScreen = NULL;
lv_obj_t *ui_WiFiKeyboard1 = NULL;
lv_obj_t *ui_WiFiPanel2 = NULL;
lv_obj_t *ui_WiFiTextArea1 = NULL;
lv_obj_t *ui_WiFiTextArea2 = NULL;
lv_obj_t *ui_WiFiLabel1 = NULL;
lv_obj_t *ui_WiFiLabel2 = NULL;
lv_obj_t *ui_WiFiPanel1 = NULL;
lv_obj_t *ui_WiFiImage1 = NULL;
lv_obj_t *ui_WiFiImage2 = NULL;
lv_obj_t *ui_WiFiLabel3 = NULL;
lv_obj_t *ui_WiFiLabel4 = NULL;

char wifi_ssid[32];
char wifi_password[32];

const char *get_wifi_ssid(void)
{
    return wifi_ssid;
}

const char *get_wifi_password(void)
{
    return wifi_password;
}

static void ui_event_WiFiTextArea1(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_FOCUSED)
    {
        _ui_basic_set_property(ui_WiFiPanel2, _UI_BASIC_PROPERTY_POSITION_Y, -80);
        _ui_keyboard_set_target(ui_WiFiKeyboard1, ui_WiFiTextArea1);
        _ui_flag_modify(ui_WiFiKeyboard1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_WiFiPanel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (event_code == LV_EVENT_DEFOCUSED)
    {
        _ui_basic_set_property(ui_WiFiPanel2, _UI_BASIC_PROPERTY_POSITION_Y, 0);
        _ui_flag_modify(ui_WiFiKeyboard1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_WiFiPanel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

static void ui_event_WiFiTextArea2(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_FOCUSED)
    {
        _ui_basic_set_property(ui_WiFiPanel2, _UI_BASIC_PROPERTY_POSITION_Y, -80);
        _ui_flag_modify(ui_WiFiKeyboard1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_WiFiPanel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_keyboard_set_target(ui_WiFiKeyboard1, ui_WiFiTextArea2);
    }
    if (event_code == LV_EVENT_DEFOCUSED)
    {
        _ui_basic_set_property(ui_WiFiPanel2, _UI_BASIC_PROPERTY_POSITION_Y, 0);
        _ui_flag_modify(ui_WiFiKeyboard1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_WiFiPanel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

static void ui_event_WiFiKeyBoard1(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_READY)
    {
        snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", lv_textarea_get_text(ui_WiFiTextArea1));
        snprintf(wifi_password, sizeof(wifi_password), "%s", lv_textarea_get_text(ui_WiFiTextArea2));

        wifi_manager_connect_with_credentials(wifi_ssid, wifi_password);
    }
}

static bool status_is_connected(const char *status)
{
    if (!status)
    {
        return false;
    }

    return (strstr(status, "connected") != NULL) || (strstr(status, "got IP") != NULL);
}

static void wifi_status_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (!ui_WiFiLoginScreen)
    {
        return;
    }

    char status[32];
    wifi_manager_get_status_text_copy(status, sizeof(status));

    if (ui_WiFiLabel3)
    {
        // Demo default text is "Not connect"; we overwrite with live status.
        lv_label_set_text(ui_WiFiLabel3, status);
    }

    if (ui_WiFiLabel4)
    {
        // Show entered SSID when available.
        if (wifi_ssid[0] != '\0')
        {
            lv_label_set_text(ui_WiFiLabel4, wifi_ssid);
        }
        else
        {
            lv_label_set_text(ui_WiFiLabel4, "SSID");
        }
    }

    bool connected = status_is_connected(status);
    if (ui_WiFiImage1 && ui_WiFiImage2)
    {
        if (connected)
        {
            lv_obj_add_flag(ui_WiFiImage1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ui_WiFiImage2, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_remove_flag(ui_WiFiImage1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_WiFiImage2, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_timer_t *s_wifi_status_timer;

static void enable_gesture_bubble_recursive(lv_obj_t *parent)
{
    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
        enable_gesture_bubble_recursive(child);
    }
}

static void ui_event_WiFiLoginScreen(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE)
    {
        return;
    }

    static uint32_t s_last_nav_ms;
    uint32_t now_ms = lv_tick_get();
    if (now_ms - s_last_nav_ms < 300)
    {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
    {
        return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_RIGHT)
    {
        s_last_nav_ms = now_ms;
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_Screen1_screen_init);
    }
}

void ui_WiFiLoginScreen_screen_init(void)
{
    ui_WiFiLoginScreen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_WiFiLoginScreen, LV_OBJ_FLAG_SCROLLABLE);

    ui_WiFiKeyboard1 = lv_keyboard_create(ui_WiFiLoginScreen);
    lv_obj_set_width(ui_WiFiKeyboard1, 318);
    lv_obj_set_height(ui_WiFiKeyboard1, 147);
    lv_obj_set_x(ui_WiFiKeyboard1, 0);
    lv_obj_set_y(ui_WiFiKeyboard1, 45);
    lv_obj_set_align(ui_WiFiKeyboard1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_WiFiKeyboard1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ui_WiFiKeyboard1, lv_color_hex(0x1A6CFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_WiFiKeyboard1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_WiFiKeyboard1, lv_color_hex(0xAAFCBB), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_WiFiKeyboard1, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_WiFiKeyboard1, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_WiFiKeyboard1, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);

    ui_WiFiPanel2 = lv_obj_create(ui_WiFiLoginScreen);
    lv_obj_set_width(ui_WiFiPanel2, 313);
    lv_obj_set_height(ui_WiFiPanel2, 97);
    lv_obj_set_x(ui_WiFiPanel2, -1);
    lv_obj_set_y(ui_WiFiPanel2, 0);
    lv_obj_set_align(ui_WiFiPanel2, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_WiFiPanel2, LV_OBJ_FLAG_SCROLLABLE);

    ui_WiFiTextArea1 = lv_textarea_create(ui_WiFiPanel2);
    lv_obj_set_width(ui_WiFiTextArea1, 178);
    lv_obj_set_height(ui_WiFiTextArea1, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiTextArea1, 55);
    lv_obj_set_y(ui_WiFiTextArea1, -22);
    lv_obj_set_align(ui_WiFiTextArea1, LV_ALIGN_CENTER);
    lv_textarea_set_placeholder_text(ui_WiFiTextArea1, "WIFI NAME");
    lv_textarea_set_one_line(ui_WiFiTextArea1, true);

    ui_WiFiTextArea2 = lv_textarea_create(ui_WiFiPanel2);
    lv_obj_set_width(ui_WiFiTextArea2, 178);
    lv_obj_set_height(ui_WiFiTextArea2, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiTextArea2, 55);
    lv_obj_set_y(ui_WiFiTextArea2, 23);
    lv_obj_set_align(ui_WiFiTextArea2, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_WiFiTextArea2, 100);
    lv_textarea_set_placeholder_text(ui_WiFiTextArea2, "PASSWORD");
    lv_textarea_set_one_line(ui_WiFiTextArea2, true);
    lv_textarea_set_password_mode(ui_WiFiTextArea2, true);

    ui_WiFiLabel1 = lv_label_create(ui_WiFiPanel2);
    lv_obj_set_width(ui_WiFiLabel1, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiLabel1, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiLabel1, -89);
    lv_obj_set_y(ui_WiFiLabel1, -20);
    lv_obj_set_align(ui_WiFiLabel1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_WiFiLabel1, "SSID");
    lv_obj_set_style_text_color(ui_WiFiLabel1, lv_color_hex(0xFF00D1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_WiFiLabel1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_WiFiLabel1, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_WiFiLabel2 = lv_label_create(ui_WiFiPanel2);
    lv_obj_set_width(ui_WiFiLabel2, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiLabel2, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiLabel2, -88);
    lv_obj_set_y(ui_WiFiLabel2, 22);
    lv_obj_set_align(ui_WiFiLabel2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_WiFiLabel2, "Password");
    lv_obj_set_style_text_color(ui_WiFiLabel2, lv_color_hex(0xFB2747), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_WiFiLabel2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_WiFiLabel2, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_WiFiPanel1 = lv_obj_create(ui_WiFiLoginScreen);
    lv_obj_set_width(ui_WiFiPanel1, 315);
    lv_obj_set_height(ui_WiFiPanel1, 32);
    lv_obj_set_x(ui_WiFiPanel1, -1);
    lv_obj_set_y(ui_WiFiPanel1, -103);
    lv_obj_set_align(ui_WiFiPanel1, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_WiFiPanel1, LV_OBJ_FLAG_SCROLLABLE);

    ui_WiFiImage1 = lv_image_create(ui_WiFiPanel1);
    lv_image_set_src(ui_WiFiImage1, &ui_img_wifierr_png);
    lv_obj_set_width(ui_WiFiImage1, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiImage1, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiImage1, -132);
    lv_obj_set_y(ui_WiFiImage1, 0);
    lv_obj_set_align(ui_WiFiImage1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_WiFiImage1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_WiFiImage1, LV_OBJ_FLAG_SCROLLABLE);

    ui_WiFiImage2 = lv_image_create(ui_WiFiPanel1);
    lv_image_set_src(ui_WiFiImage2, &ui_img_wifiok_png);
    lv_obj_set_width(ui_WiFiImage2, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiImage2, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiImage2, -132);
    lv_obj_set_y(ui_WiFiImage2, 0);
    lv_obj_set_align(ui_WiFiImage2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_WiFiImage2, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_WiFiImage2, LV_OBJ_FLAG_SCROLLABLE);

    ui_WiFiLabel3 = lv_label_create(ui_WiFiPanel1);
    lv_obj_set_width(ui_WiFiLabel3, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiLabel3, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiLabel3, 0);
    lv_obj_set_y(ui_WiFiLabel3, -2);
    lv_obj_set_align(ui_WiFiLabel3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_WiFiLabel3, "Not connect");
    lv_obj_set_style_text_color(ui_WiFiLabel3, lv_color_hex(0xFF3805), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_WiFiLabel3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_WiFiLabel3, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_WiFiLabel4 = lv_label_create(ui_WiFiPanel1);
    lv_obj_set_width(ui_WiFiLabel4, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_WiFiLabel4, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_WiFiLabel4, 124);
    lv_obj_set_y(ui_WiFiLabel4, -1);
    lv_obj_set_align(ui_WiFiLabel4, LV_ALIGN_CENTER);
    lv_label_set_text(ui_WiFiLabel4, "SSID");
    lv_obj_set_style_text_color(ui_WiFiLabel4, lv_color_hex(0x16FF05), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_WiFiLabel4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_WiFiTextArea1, ui_event_WiFiTextArea1, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_WiFiTextArea2, ui_event_WiFiTextArea2, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_WiFiKeyboard1, ui_event_WiFiKeyBoard1, LV_EVENT_ALL, NULL);

    // Periodically refresh WiFi status display while this screen exists.
    if (s_wifi_status_timer)
    {
        lv_timer_del(s_wifi_status_timer);
        s_wifi_status_timer = NULL;
    }
    s_wifi_status_timer = lv_timer_create(wifi_status_timer_cb, 500, NULL);

    // Allow swipe right to go back; does not affect visuals.
    enable_gesture_bubble_recursive(ui_WiFiLoginScreen);
    lv_obj_add_event_cb(ui_WiFiLoginScreen, ui_event_WiFiLoginScreen, LV_EVENT_GESTURE, NULL);
}

void ui_WiFiLoginScreen_screen_destroy(void)
{
    if (s_wifi_status_timer)
    {
        lv_timer_del(s_wifi_status_timer);
        s_wifi_status_timer = NULL;
    }

    if (ui_WiFiLoginScreen)
    {
        lv_obj_del(ui_WiFiLoginScreen);
    }

    ui_WiFiLoginScreen = NULL;
    ui_WiFiKeyboard1 = NULL;
    ui_WiFiPanel2 = NULL;
    ui_WiFiTextArea1 = NULL;
    ui_WiFiTextArea2 = NULL;
    ui_WiFiLabel1 = NULL;
    ui_WiFiLabel2 = NULL;
    ui_WiFiPanel1 = NULL;
    ui_WiFiImage1 = NULL;
    ui_WiFiImage2 = NULL;
    ui_WiFiLabel3 = NULL;
    ui_WiFiLabel4 = NULL;
}
