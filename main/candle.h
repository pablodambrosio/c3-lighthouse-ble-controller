#pragma once

#include "group_a.h"

// Pure time-based flame model. All positions use the selected linear RGB hue;
// each channel remains at or below its selected level. No mutable RNG state.
void candle_render(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color,
                   light_rgb_t pixels[GROUP_A_LED_COUNT]);

void candle_render_count(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color, light_rgb_t *pixels, unsigned count);
