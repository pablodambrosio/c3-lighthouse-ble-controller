#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef void *led_strip_handle_t;
typedef struct { int strip_gpio_num, max_leds, led_model, color_component_format; } led_strip_config_t;
typedef struct { int clk_src, resolution_hz, mem_block_symbols; } led_strip_rmt_config_t;
#define LED_MODEL_WS2812 0
#define LED_STRIP_COLOR_COMPONENT_FMT_GRB 0
#define RMT_CLK_SRC_DEFAULT 0
esp_err_t led_strip_new_rmt_device(const led_strip_config_t *, const led_strip_rmt_config_t *, led_strip_handle_t *);
esp_err_t led_strip_clear(led_strip_handle_t);
esp_err_t led_strip_del(led_strip_handle_t);
esp_err_t led_strip_set_pixel(led_strip_handle_t, uint32_t, uint32_t, uint32_t, uint32_t);
esp_err_t led_strip_refresh(led_strip_handle_t);
