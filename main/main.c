#include "esp_log.h"
#include "lighting.h"
#include "ble_lighting.h"
#include "light_storage.h"
#include "device_settings.h"

void app_main(void)
{
    esp_err_t err = lighting_init();
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Lighting initialization failed: %s", esp_err_to_name(err));
        return;
    }

    big_light_settings_t big_light = {
        .on = true,
        .effect = LIGHT_EFFECT_BREATHING,
        .period_ms = 1000,
        .color = light_color_from_pwm((light_rgb_t){.r = 255, .g = 120, .b = 0}),
        .brightness = 1.0f,
        .gradient_end = light_color_from_pwm((light_rgb_t){.r = 255, .g = 120, .b = 255}),
        .color_mode = LIGHT_COLOR_MONO,
        .shift_mode = LIGHT_SHIFT_STATIC,
        .shift_period_ms = 3000,
    };
    

    const big_light_settings_t big_defaults = big_light;
    err = device_settings_init();
    if (err != ESP_OK) ESP_LOGE("lighthouse", "Device settings unavailable: %s", esp_err_to_name(err));
    err = light_storage_init(&big_light);
    if (err != ESP_OK) ESP_LOGE("lighthouse", "Storage unavailable: %s", esp_err_to_name(err));
    device_settings_apply_boot(&big_light, &big_defaults);
    err = set_big_light(&big_light);
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "Could not queue big_light settings: %s", esp_err_to_name(err));
        return;
    }

    house_lights_settings_t house = {
        .on = false, .effect = LIGHT_EFFECT_SOLID, .period_ms = 4000,
        .color = light_color_from_pwm((light_rgb_t){255, 120, 0}),
        .brightness = 1.0f,
        .gradient_end = light_color_from_pwm((light_rgb_t){255, 120, 255}),
        .color_mode = LIGHT_COLOR_MONO, .shift_mode = LIGHT_SHIFT_STATIC,
        .shift_period_ms = 5000,
    };
    err = house_lights_init();
    if (err != ESP_OK) { ESP_LOGE("lighthouse", "Group B init failed: %s", esp_err_to_name(err)); return; }
    const house_lights_settings_t house_defaults = house;
    err = house_storage_init(&house);
    if (err != ESP_OK) ESP_LOGE("lighthouse", "Group B storage unavailable: %s", esp_err_to_name(err));
    device_settings_apply_boot(&house, &house_defaults);
    err = set_house_lights(&house);
    if (err != ESP_OK) { ESP_LOGE("lighthouse", "Group B settings failed: %s", esp_err_to_name(err)); return; }
    err = ble_lighting_init(&big_light, &house);
    if (err != ESP_OK) {
        ESP_LOGE("lighthouse", "BLE initialization failed: %s; local lighting remains active",
                 esp_err_to_name(err));
    }
}
