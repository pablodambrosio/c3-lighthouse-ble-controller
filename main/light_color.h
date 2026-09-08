#pragma once

#include <stdint.h>

#include "esp_err.h"

// CIE xy chromaticity only. Both coordinates must be finite and inside
// the standard sRGB primary triangle. Brightness is supplied separately.
typedef struct {
    float x;
    float y;
} light_xy_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} light_rgb_t;

// Standard sRGB/D65 reference white, without device-specific calibration.
extern const light_xy_t BIG_LIGHT_WHITE;

// Standard, gamma-encoded 8-bit sRGB input (e.g. a desktop colour picker).
light_xy_t light_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

// Raw linear LED duty cycles. Useful for migrating existing LED RGB values.
light_xy_t light_color_from_pwm(light_rgb_t pwm);

// Relative luminance Y, separately from chromaticity. Use these with the
// matching color helper to preserve an RGB/PWM input's original light output.
// Black maps to D65 xy and zero luminance.
float light_brightness_from_rgb(uint8_t r, uint8_t g, uint8_t b);
float light_brightness_from_pwm(light_rgb_t pwm);

// Validate and map xy/brightness to linear LED PWM using sRGB primaries.
// Unattainable brightness scales all channels together to preserve chromaticity.
// Invalid/out-of-gamut xy, brightness outside [0,1], or NULL output returns
// ESP_ERR_INVALID_ARG without changing output. Black uses valid xy, too.
esp_err_t light_color_to_pwm(light_xy_t color, float brightness, light_rgb_t *pwm);
