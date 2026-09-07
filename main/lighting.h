#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "light_color.h"

// Stable IDs for the future BLE protocol. Do not transmit raw C structures.
typedef enum {
    LIGHT_EFFECT_SOLID = 0,
    LIGHT_EFFECT_LIGHT_HOUSE = 1,
    LIGHT_EFFECT_CANDLE = 2,
    LIGHT_EFFECT_COLOR_LOOP = 3,
} light_effect_t;

typedef struct {
    bool on;
    light_effect_t effect;
    light_color_t color;
    uint32_t period_ms; // Full lighthouse revolution in ms; ignored for SOLID/CANDLE.
} big_light_settings_t;

typedef struct {
    bool on;
} house_lights_settings_t;

typedef struct {
    big_light_settings_t big_light;
    house_lights_settings_t house_lights;
} lighting_settings_t;

// Call once from app_main, before exposing setters to other tasks.
// Initializes Group A to black and starts the single driver-owning task.
esp_err_t lighting_init(void);

// Task-context API (including future BLE callbacks), not an ISR API.
// Copies the complete structure; the caller can reuse it after return.
// ESP_OK means queued, not yet applied to hardware. Transfers run in FIFO order.
// NULL/unknown effect/invalid xy, brightness or lighthouse period: ESP_ERR_INVALID_ARG;
// reserved effect: ESP_ERR_NOT_SUPPORTED;
// not initialized: ESP_ERR_INVALID_STATE; full queue: ESP_ERR_TIMEOUT (no wait).
// Rejected calls do not change the lights. Transfer failures are logged by the task.
// SOLID lights all LEDs. LIGHT_HOUSE crossfades between adjacent positions at
// 24 fps average, with period_ms per revolution (at least six RTOS ticks: 60 ms at 100 Hz).
// Periods round up to whole ticks and must fit within half the tick counter range.
// Fast periods may skip positions at this frame rate. Each settings
// update restarts at position 0. With on=false, sends black without switching
// MOSFET power; retain color/effect/brightness in the caller's settings to restore them.
// CANDLE uses all six positions: a wandering bright region and smooth local
// flicker at 24 fps, scaling the selected color/brightness without changing hue.
// It ignores period_ms; each settings update starts a new flame variation.
esp_err_t set_big_light(const big_light_settings_t *settings);

// Group B API reserved for its driver increment. NULL returns ESP_ERR_INVALID_ARG;
// otherwise returns ESP_ERR_NOT_SUPPORTED and does not touch hardware.
esp_err_t set_house_lights(const house_lights_settings_t *settings);
