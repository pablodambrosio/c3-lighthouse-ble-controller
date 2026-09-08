#include "light_color.h"

#include <math.h>
#include <stddef.h>

// sRGB / D65, no D50 chromatic adaptation. The map assumes linear PWM light
// output and standard primaries until the actual LEDs are characterized.
// Reference: https://registry.color.org/rgb-registry/srgb
static const float rgb_to_xyz[3][3] = {
    {0.4124564f, 0.3575761f, 0.1804375f},
    {0.2126729f, 0.7151522f, 0.0721750f},
    {0.0193339f, 0.1191920f, 0.9503041f},
};
static const float xyz_to_rgb[3][3] = {
    { 3.2404542f, -1.5371385f, -0.4985314f},
    {-0.9692660f,  1.8760108f,  0.0415560f},
    { 0.0556434f, -0.2040259f,  1.0572252f},
};

const light_xy_t BIG_LIGHT_WHITE = {
    .x = 0.3127f, .y = 0.3290f,
};

static light_xy_t from_linear(float r, float g, float b)
{
    float xyz[3];
    for (int i = 0; i < 3; ++i) {
        xyz[i] = rgb_to_xyz[i][0] * r + rgb_to_xyz[i][1] * g + rgb_to_xyz[i][2] * b;
    }
    float total = xyz[0] + xyz[1] + xyz[2];
    if (total == 0.0f) {
        // Black has no chromaticity; use D65 so the result remains valid.
        light_xy_t black = BIG_LIGHT_WHITE;
        return black;
    }
    return (light_xy_t) {
        .x = xyz[0] / total,
        .y = xyz[1] / total,
    };
}

static float srgb_to_linear(uint8_t channel)
{
    float value = channel / 255.0f;
    return value <= 0.04045f ? value / 12.92f
                           : powf((value + 0.055f) / 1.055f, 2.4f);
}

light_xy_t light_color_from_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return from_linear(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b));
}

light_xy_t light_color_from_pwm(light_rgb_t pwm)
{
    return from_linear(pwm.r / 255.0f, pwm.g / 255.0f, pwm.b / 255.0f);
}

static float luminance(float r, float g, float b)
{
    return fminf(rgb_to_xyz[1][0] * r + rgb_to_xyz[1][1] * g + rgb_to_xyz[1][2] * b, 1.0f);
}

float light_brightness_from_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return luminance(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b));
}

float light_brightness_from_pwm(light_rgb_t pwm)
{
    return luminance(pwm.r / 255.0f, pwm.g / 255.0f, pwm.b / 255.0f);
}

esp_err_t light_color_to_pwm(light_xy_t color, float brightness, light_rgb_t *pwm)
{
    if (pwm == NULL || !isfinite(color.x) || !isfinite(color.y) ||
        !isfinite(brightness) || color.x < 0.0f || color.y <= 0.0f ||
        color.x + color.y > 1.0f || brightness < 0.0f || brightness > 1.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    // XYZ at unit luminance: validate gamut even when brightness is zero.
    float xyz[3] = {color.x / color.y, 1.0f, (1.0f - color.x - color.y) / color.y};
    float rgb[3];
    for (int i = 0; i < 3; ++i) {
        float channel = xyz_to_rgb[i][0] * xyz[0] + xyz_to_rgb[i][1] * xyz[1] +
                        xyz_to_rgb[i][2] * xyz[2];
        // Allow only matrix/float roundoff at primary-triangle boundaries.
        if (!isfinite(channel) || channel < -0.00001f) {
            return ESP_ERR_INVALID_ARG;
        }
        rgb[i] = fmaxf(channel, 0.0f) * brightness;
    }

    float peak = fmaxf(1.0f, fmaxf(rgb[0], fmaxf(rgb[1], rgb[2])));
    *pwm = (light_rgb_t) {
        .r = (uint8_t)lroundf(rgb[0] / peak * 255.0f),
        .g = (uint8_t)lroundf(rgb[1] / peak * 255.0f),
        .b = (uint8_t)lroundf(rgb[2] / peak * 255.0f),
    };
    return ESP_OK;
}
