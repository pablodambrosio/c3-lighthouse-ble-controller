#include "color_pattern.h"

#include <math.h>
#include <stddef.h>

static uint32_t hash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

esp_err_t color_pattern_validate(const big_light_settings_t *settings)
{
    if (settings == NULL ||
        (settings->color_mode != LIGHT_COLOR_MONO && settings->color_mode != LIGHT_COLOR_GRADIENT) ||
        (settings->shift_mode != LIGHT_SHIFT_STATIC && settings->shift_mode != LIGHT_SHIFT_CYCLE &&
         settings->shift_mode != LIGHT_SHIFT_RANDOM) ||
        (settings->shift_mode != LIGHT_SHIFT_STATIC && settings->shift_period_ms == 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    light_rgb_t ignored;
    esp_err_t err = light_color_to_pwm(settings->color, settings->brightness, &ignored);
    if (err != ESP_OK) return err;
    if (settings->color_mode == LIGHT_COLOR_GRADIENT) {
        return light_color_to_pwm(settings->gradient_end, settings->brightness, &ignored);
    }
    return ESP_OK;
}

// Hue in linear RGB. Saturation is retained from the selected chromaticity;
// grayscale uses full saturation so a hue cycle also works when starting white.
static void hue_components(light_xy_t color, float *hue, float *saturation)
{
    light_rgb_t rgb;
    (void)light_color_to_pwm(color, 1.0f, &rgb);
    float maximum = fmaxf(rgb.r, fmaxf(rgb.g, rgb.b));
    float minimum = fminf(rgb.r, fminf(rgb.g, rgb.b));
    float delta = maximum - minimum;
    *hue = 0.0f;
    *saturation = delta == 0 ? 1.0f : delta / maximum;
    if (delta == 0) return;
    if (maximum == rgb.r) *hue = (rgb.g - rgb.b) / delta;
    else if (maximum == rgb.g) *hue = 2.0f + (rgb.b - rgb.r) / delta;
    else *hue = 4.0f + (rgb.r - rgb.g) / delta;
    *hue /= 6.0f;
    if (*hue < 0) *hue += 1.0f;
}

static light_xy_t hue_color(float hue, float saturation)
{
    hue -= floorf(hue);
    float sector = hue * 6.0f;
    float fraction = sector - floorf(sector);
    float low = 1.0f - saturation;
    float down = 1.0f - saturation * fraction;
    float up = 1.0f - saturation * (1.0f - fraction);
    float r, g, b;
    switch ((unsigned)sector % 6) {
    case 0: r = 1; g = up; b = low; break;
    case 1: r = down; g = 1; b = low; break;
    case 2: r = low; g = 1; b = up; break;
    case 3: r = low; g = down; b = 1; break;
    case 4: r = up; g = low; b = 1; break;
    default: r = 1; g = low; b = down; break;
    }
    light_xy_t color = light_color_from_pwm((light_rgb_t){
        (uint8_t)lroundf(r * 255), (uint8_t)lroundf(g * 255), (uint8_t)lroundf(b * 255)});
    return color;
}

esp_err_t color_pattern_render_count(const big_light_settings_t *settings,
                               uint64_t elapsed_ms, uint32_t seed,
                               light_rgb_t *pixels, unsigned count)
{
    float phase = settings->shift_mode == LIGHT_SHIFT_STATIC ? 0.0f :
        (elapsed_ms % settings->shift_period_ms) / (float)settings->shift_period_ms;
    float hue = 0, saturation = 0;
    if (settings->color_mode == LIGHT_COLOR_MONO && settings->shift_mode != LIGHT_SHIFT_STATIC) {
        hue_components(settings->color, &hue, &saturation);
    }
    for (unsigned i = 0; i < count; ++i) {
        float point = 0;
        if (settings->shift_mode == LIGHT_SHIFT_RANDOM) {
            uint32_t local = hash(seed ^ (0x9e3779b9u * (i + 1)));
            // Stagger jump times, with one new independent sample per period.
            uint64_t step = elapsed_ms / settings->shift_period_ms;
            uint64_t remainder = elapsed_ms % settings->shift_period_ms;
            step += (remainder + local % settings->shift_period_ms) / settings->shift_period_ms;
            uint32_t random = hash(local ^ (uint32_t)step ^ hash((uint32_t)(step >> 32)));
            point = (random & 0xffffu) / 65536.0f;
        } else if (settings->color_mode == LIGHT_COLOR_GRADIENT) {
            // A->B->A around the ring avoids a discontinuity at the seam.
            float position = i / (float)count - phase;
            position -= floorf(position);
            point = 1.0f - fabsf(2.0f * position - 1.0f);
        } else {
            point = phase;
        }
        light_xy_t color = settings->color;
        if (settings->color_mode == LIGHT_COLOR_GRADIENT) {
            color.x += (settings->gradient_end.x - color.x) * point;
            color.y += (settings->gradient_end.y - color.y) * point;
        } else if (settings->shift_mode != LIGHT_SHIFT_STATIC) {
            color = hue_color(hue + point, saturation);
        }
        float brightness = settings->on ? settings->brightness : 0.0f;
        esp_err_t err = light_color_to_pwm(color, brightness, &pixels[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t color_pattern_render(const big_light_settings_t *settings, uint64_t elapsed_ms, uint32_t seed, light_rgb_t pixels[GROUP_A_LED_COUNT])
{
    return color_pattern_render_count(settings, elapsed_ms, seed, pixels, GROUP_A_LED_COUNT);
}
