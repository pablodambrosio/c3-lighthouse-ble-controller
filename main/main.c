#include "esp_log.h"
#include "lighting.h"
#include "ble_lighting.h"

void app_main(void)
{
    esp_err_t err = lighting_init();
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Lighting initialization failed: %s", esp_err_to_name(err));
        return;
    }

    big_light_settings_t big_light = {
        .on = true,
        .effect = LIGHT_EFFECT_LIGHT_HOUSE,
        .period_ms = 10000,
        .color = light_color_from_pwm((light_rgb_t){.r = 255, .g = 120, .b = 0}),
        .brightness = 1.0f,
        .gradient_end = light_color_from_pwm((light_rgb_t){.r = 255, .g = 120, .b = 255}),
        .color_mode = LIGHT_COLOR_GRADIENT,
        .shift_mode = LIGHT_SHIFT_RANDOM,
        .shift_period_ms = 7000,
    };
    err = set_big_light(&big_light);
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Could not queue big_light settings: %s", esp_err_to_name(err));
        return;
    }
    err = ble_lighting_init(&big_light);
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "BLE initialization failed: %s; local lighting remains active",
                 esp_err_to_name(err));
    }
}
