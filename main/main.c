#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "lighting.h"
#include "ble_lighting.h"
#include "light_storage.h"
#include "device_settings.h"

#define PMIC_KEEP_ALIVE_GPIO GPIO_NUM_3 // XIAO D1
#define PMIC_KEEP_ALIVE_PERIOD_US 25000000ULL
#define PMIC_KEEP_ALIVE_PULSE_US 20000ULL

static esp_timer_handle_t pmic_release_timer;
static esp_timer_handle_t pmic_period_timer;

static void pmic_release(void *arg)
{
    (void)arg;
    // Open-drain 1 releases the pin to high impedance; it does not drive high.
    ESP_ERROR_CHECK(gpio_set_level(PMIC_KEEP_ALIVE_GPIO, 1));
}

static void pmic_pulse(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(gpio_set_level(PMIC_KEEP_ALIVE_GPIO, 0));
    ESP_ERROR_CHECK(esp_timer_start_once(pmic_release_timer, PMIC_KEEP_ALIVE_PULSE_US));
}

static void pmic_keep_alive_init(void)
{
    // Disable the output and pulls before preloading the released output level.
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << PMIC_KEEP_ALIVE_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(PMIC_KEEP_ALIVE_GPIO, 1));
    // gpio_config enables open drain before the output driver, avoiding a high glitch.
    config.mode = GPIO_MODE_OUTPUT_OD;
    ESP_ERROR_CHECK(gpio_config(&config));

    const esp_timer_create_args_t release_args = {
        .callback = pmic_release,
        .name = "pmic_release",
    };
    const esp_timer_create_args_t period_args = {
        .callback = pmic_pulse,
        .name = "pmic_period",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&release_args, &pmic_release_timer));
    ESP_ERROR_CHECK(esp_timer_create(&period_args, &pmic_period_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(pmic_period_timer, PMIC_KEEP_ALIVE_PERIOD_US));
}

void app_main(void)
{
    pmic_keep_alive_init();

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
