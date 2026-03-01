/*
 * Settings Screen – smartwatch-style scrollable settings menu.
 */

#ifndef UI_SETTINGSSCREEN_H
#define UI_SETTINGSSCREEN_H

#ifdef __cplusplus
extern "C"
{
#endif

    extern void ui_SettingsScreen_screen_init(void);
    extern void ui_SettingsScreen_screen_destroy(void);
    extern lv_obj_t *ui_SettingsScreen;
    extern lv_obj_t *ui_btnBLE;
    extern lv_obj_t *ui_lblBLE;

    /* Screen-timeout API (seconds; 0 = disabled) */
    int getScreenTimeoutSeconds(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
