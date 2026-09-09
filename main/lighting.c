#include "lighting.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "group_a.h"
#include "candle.h"
#include "sparkles.h"
#include "color_pattern.h"
#include "breathing.h"
#include <math.h>

#define LIGHTING_QUEUE_LENGTH 4
#define LIGHT_EFFECT_BLE_INDICATOR ((light_effect_t)-1)

_Static_assert(configTICK_RATE_HZ >= LIGHTING_FPS, "Frame rate exceeds RTOS tick rate");

static const char *TAG = "lighting";
static QueueHandle_t big_light_queue;

// Wide arithmetic avoids overflow before converting milliseconds to ticks.
// Reserve at least one tick per position and keep deadlines within half-range.
static TickType_t lighthouse_period_ticks(uint32_t period_ms)
{
    uint64_t scaled = (uint64_t)period_ms * configTICK_RATE_HZ;
    uint64_t ticks = (scaled + 999) / 1000;
    if (scaled < GROUP_A_LED_COUNT * 1000ULL || ticks > portMAX_DELAY / 2) {
        return 0;
    }
    return (TickType_t)ticks;
}

static uint8_t fade_channel(uint8_t channel, uint64_t fraction, TickType_t period)
{
    // Quadratic timing keeps the outgoing LED dominant longer.
    // Normalize before squaring to avoid overflow with long periods.
    float progress = (float)fraction / (float)period;
    return (uint8_t)(channel * progress * progress + 0.5f);
}

// Quadratic fade timing. Complementary integer weights preserve the total
// channel levels exactly, including at the last-to-first position boundary.
static uint8_t lighthouse_frame(TickType_t phase, TickType_t period, light_rgb_t color,
                                light_rgb_t *outgoing, light_rgb_t *incoming)
{
    uint64_t scaled = (uint64_t)(phase % period) * GROUP_A_LED_COUNT;
    uint64_t fraction = scaled % period;
    *incoming = (light_rgb_t){
        fade_channel(color.r, fraction, period),
        fade_channel(color.g, fraction, period),
        fade_channel(color.b, fraction, period),
    };
    *outgoing = (light_rgb_t){color.r - incoming->r, color.g - incoming->g,
                             color.b - incoming->b};
    return scaled / period;
}

static esp_err_t patterned_frame(const big_light_settings_t *settings, uint64_t ticks,
                                 TickType_t rotation_period, uint32_t seed)
{
    light_rgb_t pixels[GROUP_A_LED_COUNT];
    uint64_t elapsed_ms = ticks * 1000 / configTICK_RATE_HZ;
    esp_err_t err = color_pattern_render(settings, elapsed_ms, seed, pixels);
    if (err != ESP_OK) return err;
    if (settings->effect == LIGHT_EFFECT_LIGHT_HOUSE) {
        uint64_t scaled = (ticks % rotation_period) * GROUP_A_LED_COUNT;
        unsigned position = scaled / rotation_period;
        unsigned next = (position + 1) % GROUP_A_LED_COUNT;
        uint64_t fraction = scaled % rotation_period;
        for (unsigned i = 0; i < GROUP_A_LED_COUNT; ++i) {
            light_rgb_t color = pixels[i];
            light_rgb_t incoming = {
                fade_channel(color.r, fraction, rotation_period),
                fade_channel(color.g, fraction, rotation_period),
                fade_channel(color.b, fraction, rotation_period),
            };
            pixels[i] = i == next ? incoming : i == position ? (light_rgb_t){
                color.r - incoming.r, color.g - incoming.g, color.b - incoming.b
            } : (light_rgb_t){0};
        }
    } else if (settings->effect == LIGHT_EFFECT_BREATHING) {
        float level = breathing_level(elapsed_ms, settings->period_ms);
        for (unsigned i = 0; i < GROUP_A_LED_COUNT; ++i) {
            pixels[i].r = (uint8_t)lroundf(pixels[i].r * level);
            pixels[i].g = (uint8_t)lroundf(pixels[i].g * level);
            pixels[i].b = (uint8_t)lroundf(pixels[i].b * level);
        }
    } else if (settings->effect == LIGHT_EFFECT_CANDLE || settings->effect == LIGHT_EFFECT_SPARKLES) {
        light_rgb_t levels[GROUP_A_LED_COUNT];
        light_rgb_t white = {255, 255, 255};
        if (settings->effect == LIGHT_EFFECT_CANDLE) candle_render(elapsed_ms, seed, white, levels);
        else sparkles_render(elapsed_ms, seed, white, levels);
        for (unsigned i = 0; i < GROUP_A_LED_COUNT; ++i) {
            pixels[i].r = (pixels[i].r * levels[i].r + 127) / 255;
            pixels[i].g = (pixels[i].g * levels[i].r + 127) / 255;
            pixels[i].b = (pixels[i].b * levels[i].r + 127) / 255;
        }
    }
    return group_a_set_frame(pixels);
}

static void lighting_task(void *argument)
{
    QueueHandle_t queue = (QueueHandle_t)argument;
    big_light_settings_t settings = {0};
    light_rgb_t pwm = {0};
    bool animating = false;
    bool patterned = false;
    uint64_t effect_elapsed_ticks = 0;
    TickType_t effect_sampled = 0;
    uint32_t effect_seed = 0xa341316cu;
    TickType_t cycle_started = 0;
    TickType_t frame_started = 0;
    uint8_t frame_index = 0;
    TickType_t period_ticks = 0;

    bool indicating = false, connected_indicator = false;
    TickType_t indicator_started = 0;

    for (;;) {
        TickType_t wait = portMAX_DELAY;
        if (animating || indicating) {
            TickType_t elapsed = xTaskGetTickCount() - frame_started;
            TickType_t deadline = ((uint64_t)(frame_index + 1) * configTICK_RATE_HZ +
                                   LIGHTING_FPS - 1) / LIGHTING_FPS;
            wait = elapsed < deadline ? deadline - elapsed : 0;
        }

        big_light_settings_t command;
        bool received = xQueueReceive(queue, &command, wait) == pdTRUE;
        bool event = received && command.effect == LIGHT_EFFECT_BLE_INDICATOR;
        bool updated = received && !event;
        if (updated) settings = command;
        if (event) {
            indicating = true;
            connected_indicator = command.on;
            indicator_started = xTaskGetTickCount();
            frame_started = indicator_started;
            frame_index = 0;
        }
        esp_err_t err;
        if (updated) {
            float brightness = settings.brightness;
            if (!settings.on) {
                brightness = 0.0f;
            }
            err = light_color_to_pwm(settings.color, brightness, &pwm);
            if (err != ESP_OK) {
                animating = false;
                ESP_LOGE(TAG, "Invalid queued color: %s", esp_err_to_name(err));
                continue;
            }
            patterned = settings.color_mode != LIGHT_COLOR_MONO || settings.shift_mode != LIGHT_SHIFT_STATIC ||
                        settings.effect == LIGHT_EFFECT_BREATHING;
            animating = settings.on && (settings.shift_mode != LIGHT_SHIFT_STATIC ||
                                       settings.effect == LIGHT_EFFECT_LIGHT_HOUSE ||
                                       settings.effect == LIGHT_EFFECT_CANDLE ||
                                       settings.effect == LIGHT_EFFECT_SPARKLES ||
                                       settings.effect == LIGHT_EFFECT_BREATHING) &&
                       (patterned ? settings.brightness > 0 :
                        (pwm.r != 0 || pwm.g != 0 || pwm.b != 0));
            // Restart effect timing; lighthouse begins at position 0.
            period_ticks = lighthouse_period_ticks(settings.period_ms);
            cycle_started = xTaskGetTickCount();
            frame_started = cycle_started;
            frame_index = 0;
            effect_elapsed_ticks = 0;
            effect_sampled = cycle_started;
            if (settings.effect == LIGHT_EFFECT_CANDLE || settings.effect == LIGHT_EFFECT_SPARKLES ||
                settings.shift_mode == LIGHT_SHIFT_RANDOM) {
                effect_seed = (effect_seed + 0x9e3779b9u) ^ cycle_started;
            }
        } else if (animating || indicating) {
            TickType_t elapsed = xTaskGetTickCount() - frame_started;
            // Distribute fractional ticks over each second for 30 fps average.
            // Rebase whole seconds and skip obsolete frames after a delay.
            frame_started += (elapsed / configTICK_RATE_HZ) * configTICK_RATE_HZ;
            frame_index = (uint64_t)(elapsed % configTICK_RATE_HZ) * LIGHTING_FPS /
                          configTICK_RATE_HZ;
        } else {
            continue;
        }

        if (animating) {
            TickType_t now = xTaskGetTickCount();
            effect_elapsed_ticks += (TickType_t)(now - effect_sampled);
            effect_sampled = now;
        }
        if (indicating) {
            TickType_t duration = pdMS_TO_TICKS(connected_indicator ? 600 : 180);
            TickType_t elapsed = xTaskGetTickCount() - indicator_started;
            if (elapsed < duration) {
                float phase = (float)elapsed / duration;
                float level = connected_indicator ? sinf(3.14159265359f * phase) : 1.0f - phase;
                uint8_t value = (uint8_t)lroundf(128.0f * level);
                err = group_a_set_solid(connected_indicator ? (light_rgb_t){0, 0, value} :
                                                            (light_rgb_t){value, 0, 0});
                if (err == ESP_OK) continue;
                ESP_LOGE(TAG, "BLE indicator failed: %s", esp_err_to_name(err));
            }
            indicating = false;
        }
        if (patterned && settings.on && settings.brightness > 0) {
            err = patterned_frame(&settings, effect_elapsed_ticks, period_ticks, effect_seed);
        } else if (animating && (settings.effect == LIGHT_EFFECT_CANDLE ||
                                settings.effect == LIGHT_EFFECT_SPARKLES)) {
            light_rgb_t pixels[GROUP_A_LED_COUNT];
            if (settings.effect == LIGHT_EFFECT_SPARKLES) {
                sparkles_render(effect_elapsed_ticks * 1000 / configTICK_RATE_HZ,
                                effect_seed, pwm, pixels);
            } else {
                candle_render(effect_elapsed_ticks * 1000 / configTICK_RATE_HZ,
                              effect_seed, pwm, pixels);
            }
            err = group_a_set_frame(pixels);
        } else if (animating) {
            TickType_t elapsed = xTaskGetTickCount() - cycle_started;
            cycle_started += (elapsed / period_ticks) * period_ticks;
            light_rgb_t outgoing, incoming;
            uint8_t position = lighthouse_frame(elapsed % period_ticks, period_ticks, pwm,
                                                &outgoing, &incoming);
            err = group_a_set_pair(position, outgoing, incoming);
        } else {
            err = group_a_set_solid(pwm);
        }
        if (err != ESP_OK) {
            animating = false;
            ESP_LOGE(TAG, "big_light update failed: %s; LEDs may retain their previous frame",
                     esp_err_to_name(err));
            continue;
        }
        if (updated) {
            ESP_LOGI(TAG, "big_light applied: on=%d, effect=%d, period_ms=%lu, xy=(%.5f,%.5f), B=%.5f, PWM=(%u,%u,%u), color_mode=%d, shift_mode=%d, shift_period_ms=%lu",
                 (int)settings.on, (int)settings.effect,
                 (unsigned long)settings.period_ms,
                 (double)settings.color.x, (double)settings.color.y,
                 (double)(settings.on ? settings.brightness : 0.0f),
                 (unsigned)pwm.r, (unsigned)pwm.g, (unsigned)pwm.b,
                 (int)settings.color_mode, (int)settings.shift_mode,
                 (unsigned long)settings.shift_period_ms);
        }
    }
}

esp_err_t lighting_init(void)
{
    if (big_light_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    QueueHandle_t queue = xQueueCreate(LIGHTING_QUEUE_LENGTH, sizeof(big_light_settings_t));
    if (queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Hardware initialization is serialized before the task takes ownership.
    esp_err_t err = group_a_init();
    if (err != ESP_OK) {
        vQueueDelete(queue);
        return err;
    }

    if (xTaskCreate(lighting_task, "lighting", 3072, queue, 5, NULL) != pdPASS) {
        esp_err_t cleanup_err = group_a_deinit();
        if (cleanup_err != ESP_OK) {
            ESP_LOGE(TAG, "Group A cleanup failed: %s", esp_err_to_name(cleanup_err));
        }
        vQueueDelete(queue);
        return ESP_ERR_NO_MEM;
    }

    big_light_queue = queue;
    return ESP_OK;
}

esp_err_t lighting_validate(const big_light_settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (settings->effect) {
    case LIGHT_EFFECT_SOLID:
    case LIGHT_EFFECT_CANDLE:
    case LIGHT_EFFECT_SPARKLES:
        break;
    case LIGHT_EFFECT_LIGHT_HOUSE:
    case LIGHT_EFFECT_BREATHING:
        if (lighthouse_period_ticks(settings->period_ms) == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = color_pattern_validate(settings);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t set_big_light(const big_light_settings_t *settings)
{
    esp_err_t err = lighting_validate(settings);
    if (err != ESP_OK) return err;
    if (big_light_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // FreeRTOS copies sizeof(big_light_settings_t) bytes into the queue.
    return xQueueSend(big_light_queue, settings, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}


esp_err_t lighting_ble_indicator(bool connected)
{
    if (!big_light_queue) return ESP_ERR_INVALID_STATE;
    const big_light_settings_t event = {.effect = LIGHT_EFFECT_BLE_INDICATOR, .on = connected};
    return xQueueSend(big_light_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
