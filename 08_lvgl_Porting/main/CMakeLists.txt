idf_component_register(
    SRCS
        "wave_rgb_lcd/waveshare_rgb_lcd_port.c"
        "main.c"
        "lvgl_port/lvgl_port.c"
        "ui/ui_common.c"
        "ui/user_session.c"
        "ui/ui_rfid.c"
        "ui/ui_login.c"
        "infrared/infrared.c"
        "ui/ui_map.c"
        "ui/ui_floor_map_assets.c"
        "ui/ui_search.c"
        "ui/ui_qr_login.c"
        "ui/library_ui.c"
        "ui/ui_wifi.c"
        "ui/ui_borrow.c"
        "pn532/pn532_app.c"
        "wifi/wifi_config.c"
        "http_client/http_client.c"
    INCLUDE_DIRS
        "."
        "ui"
        "pn532"
        "infrared"
        "../components/local_db"
        "http_client"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        json
        esp_http_client
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
