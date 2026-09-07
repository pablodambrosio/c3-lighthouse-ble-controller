#include "esp_log.h"
#include "lighting.h"

void app_main(void)
{
    esp_err_t err = lighting_init();
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Lighting initialization failed: %s", esp_err_to_name(err));
        return;
    }

    big_light_settings_t big_light = {
        .on = true,
        .effect = LIGHT_EFFECT_CANDLE,
        .period_ms = 15000,
        // Preserve the current raw LED setting while moving control to xyB.
        // For standard colour-picker RGB, use light_color_from_rgb(r, g, b).
        .color = light_color_from_pwm((light_rgb_t){.r = 255, .g = 100, .b = 0}),
    };
    err = set_big_light(&big_light);
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Could not queue big_light settings: %s", esp_err_to_name(err));
    }
}
