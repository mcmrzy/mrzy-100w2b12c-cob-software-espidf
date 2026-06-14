idf_component_register(
    SRCS
        "main.cpp"
        "storage/storage.cpp"
        "led/led_control.cpp"
        "sensor/sensors.cpp"
        "display/st7789v.cpp"
        "display/lvgl_disp.cpp"
        "display/lvgl_indev.cpp"
        "display/lvgl_ui.cpp"
        "display/ui_page_mgr.cpp"
        "display/ui_dashboard.cpp"
        "display/ui_control.cpp"
        "display/ui_sensors.cpp"
        "display/ui_settings.cpp"
        # "network/web_ui.cpp"      /* 暂时禁用 */
        # "network/ble_ui.cpp"      /* 暂时禁用 */
        # "network/mqtt_client.cpp" /* 暂时禁用 */
        # "network/espnow.cpp"      /* 暂时禁用 */
        # "system/sleep.cpp"        /* 暂时禁用 */
        # "system/ota.cpp"          /* 暂时禁用 */
        # "system/app_console.cpp"  /* 暂时禁用 */
        # "system/audio.cpp"        /* 暂时禁用 */
    INCLUDE_DIRS
        "."
        "hal"
        "storage"
        "led"
        "sensor"
        "display"
        "network"
        "system"
    REQUIRES
        esp_timer
        esp_event
        esp_wifi
        esp_system
        nvs_flash
        driver
        esp_driver_mcpwm
        esp_adc
        lvgl__lvgl
)
