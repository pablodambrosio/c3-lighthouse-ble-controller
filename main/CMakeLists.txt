idf_component_register(SRCS "light_color.c" "main.c" "group_a.c" "lighting.c"
                       "candle.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio led_strip log freertos)
