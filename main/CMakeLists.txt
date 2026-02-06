file(GLOB WATCH_UI_SOURCES watch_ui/*.c)
list(FILTER WATCH_UI_SOURCES EXCLUDE REGEX "watch_ui/ui_WiFiScreen\\.c$")

idf_component_register(
    SRCS main.c battery_monitor.c wifi_manager.c ${WATCH_UI_SOURCES}
    INCLUDE_DIRS . watch_ui
    PRIV_REQUIRES driver nvs_flash esp_wifi esp_event esp_netif
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
