#include "lighting.h"
#include "breathing.h"
#include "color_pattern.h"
#include "candle.h"
#include "sparkles.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>

#define GROUP_B_LED_COUNT 4
static QueueHandle_t queue;
static led_strip_handle_t strip;
static void owner(void *arg)
{
    (void)arg;
    house_lights_settings_t s = {0};
    bool active = false;
    uint64_t ticks = 0;
    TickType_t sampled = 0, frame_start = 0;
    unsigned frame = 0;
    uint32_t seed = 0x76543210;
    for (;;) {
        TickType_t wait = portMAX_DELAY;
        if (active) {
            TickType_t elapsed = xTaskGetTickCount() - frame_start;
            TickType_t deadline = ((uint64_t)(frame + 1) * configTICK_RATE_HZ + LIGHTING_FPS - 1) / LIGHTING_FPS;
            wait = elapsed < deadline ? deadline - elapsed : 0;
        }
        bool updated = xQueueReceive(queue, &s, wait) == pdTRUE;
        TickType_t now = xTaskGetTickCount();
        if (updated) {
            ticks = 0; sampled = frame_start = now; frame = 0;
            seed = (seed + 0x9e3779b9u) ^ now;
            active = s.on && s.brightness > 0 && (s.effect != LIGHT_EFFECT_SOLID || s.shift_mode != LIGHT_SHIFT_STATIC);
        } else if (!active) continue;
        else {
            ticks += (TickType_t)(now - sampled); sampled = now;
            TickType_t elapsed = now - frame_start;
            frame_start += elapsed / configTICK_RATE_HZ * configTICK_RATE_HZ;
            frame = (uint64_t)(elapsed % configTICK_RATE_HZ) * LIGHTING_FPS / configTICK_RATE_HZ;
        }
        uint64_t ms = ticks * 1000 / configTICK_RATE_HZ;
        light_rgb_t pixels[GROUP_B_LED_COUNT], levels[GROUP_B_LED_COUNT];
        esp_err_t err = color_pattern_render_count(&s, ms, seed, pixels, GROUP_B_LED_COUNT);
        if (err == ESP_OK) {
            if (s.effect == LIGHT_EFFECT_CANDLE) candle_render_count(ms, seed, (light_rgb_t){255,255,255}, levels, GROUP_B_LED_COUNT);
            if (s.effect == LIGHT_EFFECT_SPARKLES) sparkles_render_count(ms, seed, (light_rgb_t){255,255,255}, levels, GROUP_B_LED_COUNT);
            float breath = s.effect == LIGHT_EFFECT_BREATHING ? breathing_level(ms, s.period_ms) : 1;
            for (unsigned i = 0; i < GROUP_B_LED_COUNT && err == ESP_OK; ++i) {
                float level = s.effect == LIGHT_EFFECT_CANDLE || s.effect == LIGHT_EFFECT_SPARKLES ? levels[i].r / 255.0f : breath;
                err = led_strip_set_pixel(strip, i, lroundf(pixels[i].r * level), lroundf(pixels[i].g * level), lroundf(pixels[i].b * level));
            }
            if (err == ESP_OK) err = led_strip_refresh(strip);
        }
        if (err != ESP_OK) { active = false; ESP_LOGE("group_b", "Render failed: %s", esp_err_to_name(err)); }
    }
}
esp_err_t house_lights_init(void)
{
    if (queue) return ESP_ERR_INVALID_STATE;
    led_strip_config_t config = {.strip_gpio_num = GPIO_NUM_6, .max_leds = GROUP_B_LED_COUNT,
        .led_model = LED_MODEL_WS2812, .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB};
    led_strip_rmt_config_t rmt = {.clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10000000, .mem_block_symbols = 48};
    esp_err_t err = led_strip_new_rmt_device(&config, &rmt, &strip);
    if (err != ESP_OK) return err;
    err = led_strip_clear(strip);
    if (err != ESP_OK) { led_strip_del(strip); strip = NULL; return err; }
    queue = xQueueCreate(4, sizeof(house_lights_settings_t));
    if (!queue || xTaskCreate(owner, "house_lights", 3072, NULL, 5, NULL) != pdPASS) {
        if (queue) vQueueDelete(queue);
        queue = NULL; led_strip_del(strip); strip = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t set_house_lights(const house_lights_settings_t *s)
{
    if (!s) return ESP_ERR_INVALID_ARG;
    if (s->effect == LIGHT_EFFECT_LIGHT_HOUSE) return ESP_ERR_NOT_SUPPORTED;
    esp_err_t err = lighting_validate(s);
    if (err != ESP_OK) return err;
    if (!queue) return ESP_ERR_INVALID_STATE;
    return xQueueSend(queue, s, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
