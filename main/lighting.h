#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "light_color.h"

// Stable IDs for the BLE protocol. Do not transmit raw C structures.
typedef enum {
    LIGHT_EFFECT_SOLID = 0,
    LIGHT_EFFECT_LIGHT_HOUSE = 1,
    LIGHT_EFFECT_CANDLE = 2,
    LIGHT_EFFECT_SPARKLES = 4,
} light_effect_t;

typedef enum {
    LIGHT_COLOR_MONO = 0,
    LIGHT_COLOR_GRADIENT = 1,
} light_color_mode_t;

typedef enum {
    LIGHT_SHIFT_STATIC = 0,
    LIGHT_SHIFT_CYCLE = 1,
    LIGHT_SHIFT_RANDOM = 2,
} light_shift_mode_t;

typedef struct {
    bool on;
    light_effect_t effect;
    light_xy_t color;
    float brightness; // Shared relative luminance Y: 0 = black, 1 = reference white.
    uint32_t period_ms; // Full lighthouse revolution in ms; ignored for other effects.
    light_color_mode_t color_mode;
    light_shift_mode_t shift_mode;
    light_xy_t gradient_end; // Second xy endpoint; shares brightness. Ignored in MONO.
    uint32_t shift_period_ms; // Full cycle or each LED's jump interval; ignored in STATIC.
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

// Task-context API (including BLE callbacks), not an ISR API.
// Copies the complete structure; the caller can reuse it after return.
// ESP_OK means queued, not yet applied to hardware. Transfers run in FIFO order.
// NULL/unknown effect or mode/invalid xy, brightness or applicable period:
// ESP_ERR_INVALID_ARG;
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
// SPARKLES independently brightens and fades each LED at 24 fps, using the
// selected color/brightness. Ignores period_ms; updates start a new variation.
// Color modes are independent of effect: MONO uses color; GRADIENT interpolates
// color.xy -> gradient_end -> color.xy around the ring, at the shared brightness.
// STATIC holds colors. CYCLE sweeps a full hue turn in MONO or rotates the
// gradient. RANDOM picks independent colors at staggered intervals per LED.
// Nonstatic shift_period_ms must be >0 (full cycle / per-LED jump interval).
// Zero-initialized additional fields preserve MONO + STATIC. Unused gradient
// endpoints and static shift periods are ignored. Updates restart both clocks.
esp_err_t set_big_light(const big_light_settings_t *settings);

// Group B API reserved for its driver increment. NULL returns ESP_ERR_INVALID_ARG;
// otherwise returns ESP_ERR_NOT_SUPPORTED and does not touch hardware.
esp_err_t set_house_lights(const house_lights_settings_t *settings);
