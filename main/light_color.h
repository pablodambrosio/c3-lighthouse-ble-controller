#pragma once

#include <stdint.h>

#include "esp_err.h"

// CIE xy chromaticity and relative luminance Y (called brightness here).
// brightness: 0.0 = black, 1.0 = reference white luminance. All fields must
// be finite; x/y must lie inside the standard sRGB primary triangle.
typedef struct {
    float x;
    float y;
    float brightness;
} light_color_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} light_rgb_t;

// Standard sRGB/D65 reference white, without device-specific calibration.
extern const light_color_t BIG_LIGHT_WHITE;

// Standard, gamma-encoded 8-bit sRGB input (e.g. a desktop colour picker).
light_color_t light_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

// Raw linear LED duty cycles. Useful for migrating existing LED RGB values.
light_color_t light_color_from_pwm(light_rgb_t pwm);

// Validate and map xy/brightness to linear LED PWM using sRGB primaries.
// Unattainable brightness scales all channels together to preserve chromaticity.
// Invalid/out-of-gamut xy, brightness outside [0,1], or NULL output returns
// ESP_ERR_INVALID_ARG without changing output. Black uses valid xy, too.
esp_err_t light_color_to_pwm(light_color_t color, light_rgb_t *pwm);
