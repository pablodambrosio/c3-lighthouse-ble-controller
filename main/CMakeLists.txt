idf_component_register(SRCS "light_color.c" "main.c" "group_a.c" "lighting.c"
                       "candle.c" "sparkles.c" "color_pattern.c"
                       "ble_lighting.c" "ble_light_protocol.c" "light_storage.c" "group_b.c" "device_settings.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio led_strip log freertos bt nvs_flash)
