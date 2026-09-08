#pragma once

#include "group_a.h"

// Validate modes, applicable gradient endpoint and nonstatic shift timing.
esp_err_t color_pattern_validate(const big_light_settings_t *settings);

// Pure color generation before lighting-effect intensity is applied.
// Caller supplies validated settings and elapsed milliseconds since update.
esp_err_t color_pattern_render(const big_light_settings_t *settings,
                               uint64_t elapsed_ms, uint32_t seed,
                               light_rgb_t pixels[GROUP_A_LED_COUNT]);
