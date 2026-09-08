#pragma once

#include "group_a.h"

// Independent, time-based pulses; scales the selected PWM without changing hue.
void sparkles_render(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color,
                     light_rgb_t pixels[GROUP_A_LED_COUNT]);
