#include <math.h>
#include <stddef.h>
#include <reent.h>
#include "light_color.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

// Supply newlib's errno storage for this single-threaded test runtime.
static struct _reent test_reent; // Only errno is used; no stdio in these tests.
struct _reent *__getreent(void)
{
    return &test_reent;
}

static int matches(light_rgb_t a, light_rgb_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int main(void)
{
    const struct {
        light_rgb_t rgb;
        float x, y, luminance;
    } references[] = {
        {{255, 0, 0}, 0.64f, 0.33f, 0.212673f},
        {{0, 255, 0}, 0.30f, 0.60f, 0.715152f},
        {{0, 0, 255}, 0.15f, 0.06f, 0.072175f},
        {{255, 255, 255}, 0.3127f, 0.3290f, 1.0f},
        {{255, 255, 0}, 0.41932f, 0.50525f, 0.927825f},
    };
    light_rgb_t pwm;
    for (unsigned i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        light_rgb_t rgb = references[i].rgb;
        light_xy_t color = light_color_from_rgb(rgb.r, rgb.g, rgb.b);
        float brightness = light_brightness_from_rgb(rgb.r, rgb.g, rgb.b);
        CHECK(fabsf(color.x - references[i].x) < 0.00005f);
        CHECK(fabsf(color.y - references[i].y) < 0.00005f);
        CHECK(fabsf(brightness - references[i].luminance) < 0.00001f);
        CHECK(light_color_to_pwm(color, brightness, &pwm) == ESP_OK);
        CHECK(matches(rgb, pwm));
    }
    // Standard sRGB mid-grey is about 21.6% linear light.
    const uint8_t greys[][2] = {{0, 0}, {10, 1}, {11, 1}, {64, 13}, {128, 55}, {255, 255}};
    for (unsigned i = 0; i < sizeof(greys) / sizeof(greys[0]); ++i) {
        uint8_t v = greys[i][0], expected = greys[i][1];
        CHECK(light_color_to_pwm(light_color_from_rgb(v, v, v), light_brightness_from_rgb(v, v, v), &pwm) == ESP_OK);
        CHECK(matches(pwm, ((light_rgb_t){expected, expected, expected})));
    }
    for (int r = 0; r <= 255; r += 17) {
        for (int g = 0; g <= 255; g += 17) {
            for (int b = 0; b <= 255; b += 17) {
                light_rgb_t rgb = {r, g, b};
                CHECK(light_color_to_pwm(light_color_from_pwm(rgb), light_brightness_from_pwm(rgb), &pwm) == ESP_OK);
                CHECK(matches(rgb, pwm));
            }
        }
    }
    uint32_t seed = 42;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        light_rgb_t rgb = {seed >> 24, seed >> 16, seed >> 8};
        CHECK(light_color_to_pwm(light_color_from_pwm(rgb), light_brightness_from_pwm(rgb), &pwm) == ESP_OK);
        CHECK(matches(rgb, pwm));
    }
    light_rgb_t startup = {255, 200, 0};
    CHECK(light_color_to_pwm(light_color_from_pwm(startup), light_brightness_from_pwm(startup), &pwm) == ESP_OK);
    CHECK(matches(startup, pwm));

    light_xy_t white = BIG_LIGHT_WHITE;
    CHECK(light_color_to_pwm(white, 1.0f, &pwm) == ESP_OK);
    CHECK(matches(pwm, ((light_rgb_t){255, 255, 255})));
    CHECK(light_color_to_pwm(white, 0.5f, &pwm) == ESP_OK);
    CHECK(pwm.r >= 127 && pwm.r <= 128 && pwm.g >= 127 && pwm.g <= 128 &&
          pwm.b >= 127 && pwm.b <= 128);
    CHECK(light_color_to_pwm(white, 0.0f, &pwm) == ESP_OK);
    CHECK(matches(pwm, ((light_rgb_t){0, 0, 0})));
    light_xy_t black = light_color_from_rgb(0, 0, 0);
    CHECK(black.y > 0.0f && light_brightness_from_rgb(0, 0, 0) == 0.0f);

    light_xy_t limited = light_color_from_pwm((light_rgb_t){255, 128, 64});
    CHECK(light_color_to_pwm(limited, 1.0f, &pwm) == ESP_OK);
    CHECK(matches(pwm, ((light_rgb_t){255, 128, 64})));

    const struct { float x, y, brightness; } invalid[] = {
        {0.3f, 0.0f, 1}, {-0.1f, 0.3f, 1}, {0.8f, 0.8f, 1},
        {0.1f, 0.8f, 1}, {0.1f, 0.8f, 0}, // Outside sRGB, even when off.
        {0.3127f, 0.329f, -0.1f}, {0.3127f, 0.329f, 1.1f}, {0.3f, 1e-40f, 1},
        {NAN, 0.329f, 1}, {0.3127f, NAN, 1}, {0.3127f, 0.329f, NAN},
        {INFINITY, 0.329f, 1}, {0.3127f, INFINITY, 1}, {0.3127f, 0.329f, INFINITY},
        {-INFINITY, 0.329f, 1}, {0.3127f, -INFINITY, 1}, {0.3127f, 0.329f, -INFINITY},
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        pwm = (light_rgb_t){12, 34, 56};
        CHECK(light_color_to_pwm((light_xy_t){invalid[i].x, invalid[i].y}, invalid[i].brightness, &pwm) == ESP_ERR_INVALID_ARG);
        CHECK(matches(pwm, ((light_rgb_t){12, 34, 56})));
    }
    CHECK(light_color_to_pwm(BIG_LIGHT_WHITE, 1.0f, NULL) == ESP_ERR_INVALID_ARG);
    return 0;
}
