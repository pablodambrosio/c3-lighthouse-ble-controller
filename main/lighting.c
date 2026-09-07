#include "lighting.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "group_a.h"
#include "candle.h"

#define LIGHTING_QUEUE_LENGTH 4
#define LIGHTING_FPS 24

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
    return (channel * fraction + period / 2) / period;
}

// Linear light interpolation. Complementary integer weights preserve the total
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

static void lighting_task(void *argument)
{
    QueueHandle_t queue = (QueueHandle_t)argument;
    big_light_settings_t settings;
    light_rgb_t pwm = {0};
    bool animating = false;
    uint64_t candle_elapsed_ticks = 0;
    TickType_t candle_sampled = 0;
    uint32_t candle_seed = 0xa341316cu;
    TickType_t cycle_started = 0;
    TickType_t frame_started = 0;
    uint8_t frame_index = 0;
    TickType_t period_ticks = 0;

    for (;;) {
        TickType_t wait = portMAX_DELAY;
        if (animating) {
            TickType_t elapsed = xTaskGetTickCount() - frame_started;
            TickType_t deadline = ((uint64_t)(frame_index + 1) * configTICK_RATE_HZ +
                                   LIGHTING_FPS - 1) / LIGHTING_FPS;
            wait = elapsed < deadline ? deadline - elapsed : 0;
        }

        bool updated = xQueueReceive(queue, &settings, wait) == pdTRUE;
        esp_err_t err;
        if (updated) {
            light_color_t color = settings.color;
            if (!settings.on) {
                color.brightness = 0.0f;
            }
            err = light_color_to_pwm(color, &pwm);
            if (err != ESP_OK) {
                animating = false;
                ESP_LOGE(TAG, "Invalid queued color: %s", esp_err_to_name(err));
                continue;
            }
            animating = settings.on && (settings.effect == LIGHT_EFFECT_LIGHT_HOUSE ||
                                       settings.effect == LIGHT_EFFECT_CANDLE) &&
                       (pwm.r != 0 || pwm.g != 0 || pwm.b != 0);
            // Restart effect timing; lighthouse begins at position 0.
            period_ticks = lighthouse_period_ticks(settings.period_ms);
            cycle_started = xTaskGetTickCount();
            frame_started = cycle_started;
            frame_index = 0;
            candle_elapsed_ticks = 0;
            candle_sampled = cycle_started;
            if (settings.effect == LIGHT_EFFECT_CANDLE) {
                candle_seed = (candle_seed + 0x9e3779b9u) ^ cycle_started;
            }
        } else if (animating) {
            TickType_t elapsed = xTaskGetTickCount() - frame_started;
            // Distribute fractional ticks over each second for 24 fps average.
            // Rebase whole seconds and skip obsolete frames after a delay.
            frame_started += (elapsed / configTICK_RATE_HZ) * configTICK_RATE_HZ;
            frame_index = (uint64_t)(elapsed % configTICK_RATE_HZ) * LIGHTING_FPS /
                          configTICK_RATE_HZ;
        } else {
            continue;
        }

        if (animating && settings.effect == LIGHT_EFFECT_CANDLE) {
            TickType_t now = xTaskGetTickCount();
            candle_elapsed_ticks += (TickType_t)(now - candle_sampled);
            candle_sampled = now;
            light_rgb_t pixels[GROUP_A_LED_COUNT];
            candle_render(candle_elapsed_ticks * 1000 / configTICK_RATE_HZ,
                          candle_seed, pwm, pixels);
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
            ESP_LOGI(TAG, "big_light applied: on=%d, effect=%d, period_ms=%lu, xy=(%.5f,%.5f), B=%.5f, PWM=(%u,%u,%u)",
                 (int)settings.on, (int)settings.effect,
                 (unsigned long)settings.period_ms,
                 (double)settings.color.x, (double)settings.color.y,
                 (double)(settings.on ? settings.color.brightness : 0.0f),
                 (unsigned)pwm.r, (unsigned)pwm.g, (unsigned)pwm.b);
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

esp_err_t set_big_light(const big_light_settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (settings->effect) {
    case LIGHT_EFFECT_SOLID:
    case LIGHT_EFFECT_CANDLE:
        break;
    case LIGHT_EFFECT_LIGHT_HOUSE:
        if (lighthouse_period_ticks(settings->period_ms) == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case LIGHT_EFFECT_COLOR_LOOP:
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    light_rgb_t pwm;
    esp_err_t err = light_color_to_pwm(settings->color, &pwm);
    if (err != ESP_OK) {
        return err;
    }
    if (big_light_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // FreeRTOS copies sizeof(big_light_settings_t) bytes into the queue.
    return xQueueSend(big_light_queue, settings, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t set_house_lights(const house_lights_settings_t *settings)
{
    return settings == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_SUPPORTED;
}
