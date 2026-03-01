file(GLOB WATCH_UI_SOURCES watch_ui/*.c)

idf_component_register(
    SRCS main.c battery_monitor.c ble_server.c ${WATCH_UI_SOURCES}
    INCLUDE_DIRS . watch_ui
    PRIV_REQUIRES driver nvs_flash bt esp_wifi
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
