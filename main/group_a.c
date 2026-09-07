#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "group_a.h"
#include "led_strip.h"

#define GROUP_A_DATA_GPIO GPIO_NUM_4

typedef enum {
    POSITION_0,
    POSITION_1,
    POSITION_2,
    POSITION_3,
    POSITION_4,
    POSITION_5,
} group_a_position_t;

// Six perimeter LEDs, with no center LED. Positions default to chain order;
// change only this table if the physical DOUT-to-DIN chain order differs.
static const uint8_t chain_index[GROUP_A_LED_COUNT] = {
    [POSITION_0] = 0, [POSITION_1] = 1, [POSITION_2] = 2,
    [POSITION_3] = 3, [POSITION_4] = 4, [POSITION_5] = 5,
};

static const char *TAG = "group_a";
static led_strip_handle_t strip;

esp_err_t group_a_init(void)
{
    if (strip != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const led_strip_config_t config = {
        .strip_gpio_num = GROUP_A_DATA_GPIO,
        .max_leds = GROUP_A_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 48,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&config, &rmt_config, &strip);
    if (err != ESP_OK) {
        return err;
    }

    // Start at black. Power-enable GPIO5 remains unused until phase 3.
    err = led_strip_clear(strip);
    if (err != ESP_OK) {
        esp_err_t cleanup_err = group_a_deinit();
        if (cleanup_err != ESP_OK) {
            ESP_LOGE(TAG, "Initialization cleanup failed: %s", esp_err_to_name(cleanup_err));
        }
    }
    return err;
}

esp_err_t group_a_deinit(void)
{
    if (strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = led_strip_del(strip);
    if (err == ESP_OK) {
        strip = NULL;
    }
    return err;
}

esp_err_t group_a_set_solid(light_rgb_t pwm)
{
    if (strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int position = 0; position < GROUP_A_LED_COUNT; ++position) {
        esp_err_t err = led_strip_set_pixel(strip, chain_index[position],
                                           pwm.r, pwm.g, pwm.b);
        if (err != ESP_OK) {
            return err;
        }
    }

    return led_strip_refresh(strip);
}

esp_err_t group_a_set_single(uint8_t position, light_rgb_t pwm)
{
    return group_a_set_pair(position, pwm, (light_rgb_t){0});
}

esp_err_t group_a_set_pair(uint8_t position, light_rgb_t outgoing, light_rgb_t incoming)
{
    if (position >= GROUP_A_LED_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t next = (position + 1) % GROUP_A_LED_COUNT;
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        light_rgb_t pixel = i == position ? outgoing : i == next ? incoming : (light_rgb_t){0};
        esp_err_t err = led_strip_set_pixel(strip, chain_index[i], pixel.r, pixel.g, pixel.b);
        if (err != ESP_OK) {
            return err;
        }
    }
    return led_strip_refresh(strip);
}

esp_err_t group_a_set_frame(const light_rgb_t pixels[GROUP_A_LED_COUNT])
{
    if (pixels == NULL) return ESP_ERR_INVALID_ARG;
    if (strip == NULL) return ESP_ERR_INVALID_STATE;
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        esp_err_t err = led_strip_set_pixel(strip, chain_index[i], pixels[i].r,
                                           pixels[i].g, pixels[i].b);
        if (err != ESP_OK) return err;
    }
    return led_strip_refresh(strip);
}
