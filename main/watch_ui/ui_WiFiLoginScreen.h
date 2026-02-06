// WiFi login screen (ported from wifi_login_demo attachment)

#ifndef UI_WIFILOGINSCREEN_H
#define UI_WIFILOGINSCREEN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

    // SCREEN: ui_WiFiLoginScreen
    void ui_WiFiLoginScreen_screen_init(void);
    void ui_WiFiLoginScreen_screen_destroy(void);
    extern lv_obj_t *ui_WiFiLoginScreen;

    extern lv_obj_t *ui_WiFiKeyboard1;
    extern lv_obj_t *ui_WiFiPanel2;
    extern lv_obj_t *ui_WiFiTextArea1;
    extern lv_obj_t *ui_WiFiTextArea2;
    extern lv_obj_t *ui_WiFiLabel1;
    extern lv_obj_t *ui_WiFiLabel2;
    extern lv_obj_t *ui_WiFiPanel1;
    extern lv_obj_t *ui_WiFiImage1;
    extern lv_obj_t *ui_WiFiImage2;
    extern lv_obj_t *ui_WiFiLabel3;
    extern lv_obj_t *ui_WiFiLabel4;

    extern char wifi_ssid[32];
    extern char wifi_password[32];

    const char *get_wifi_ssid(void);
    const char *get_wifi_password(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
