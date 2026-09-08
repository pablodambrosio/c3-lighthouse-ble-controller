#include <reent.h>
#include <math.h>
#include <string.h>
#include "color_pattern.h"

static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    big_light_settings_t s = {.on = true, .color = {0.64f, 0.33f}, .brightness = 0.3f};
    light_rgb_t pixels[6], reference[6], other[6], expected;
    CHECK(color_pattern_validate(NULL) == ESP_ERR_INVALID_ARG);
    CHECK(color_pattern_validate(&s) == ESP_OK);
    CHECK(color_pattern_render(&s, 0, 42, pixels) == ESP_OK);
    CHECK(light_color_to_pwm(s.color, s.brightness, &expected) == ESP_OK);
    for (int i = 0; i < 6; ++i) CHECK(memcmp(&pixels[i], &expected, sizeof(expected)) == 0);
    CHECK(color_pattern_render(&s, UINT64_C(5000000000), 99, other) == ESP_OK);
    CHECK(memcmp(pixels, other, sizeof(pixels)) == 0);

    s.color_mode = LIGHT_COLOR_GRADIENT;
    CHECK(color_pattern_validate(&s) == ESP_ERR_INVALID_ARG);
    s.gradient_end = (light_xy_t){0.15f, 0.06f};
    CHECK(color_pattern_validate(&s) == ESP_OK);
    CHECK(color_pattern_render(&s, 0, 42, reference) == ESP_OK);
    for (int i = 0; i < 6; ++i) {
        float amount = i <= 3 ? i / 3.0f : (6 - i) / 3.0f;
        light_xy_t xy = {s.color.x + (s.gradient_end.x - s.color.x) * amount,
                            s.color.y + (s.gradient_end.y - s.color.y) * amount};
        CHECK(light_color_to_pwm(xy, s.brightness, &expected) == ESP_OK);
        CHECK(memcmp(&reference[i], &expected, sizeof(expected)) == 0);
    }
    s.shift_mode = LIGHT_SHIFT_CYCLE;
    CHECK(color_pattern_validate(&s) == ESP_ERR_INVALID_ARG);
    s.shift_period_ms = 6000;
    CHECK(color_pattern_validate(&s) == ESP_OK);
    CHECK(color_pattern_render(&s, 1000, 42, pixels) == ESP_OK);
    for (int i = 0; i < 6; ++i) CHECK(memcmp(&pixels[i], &reference[(i + 5) % 6], sizeof(expected)) == 0);
    CHECK(color_pattern_render(&s, 6000, 42, pixels) == ESP_OK);
    CHECK(memcmp(pixels, reference, sizeof(pixels)) == 0);

    s.color_mode = LIGHT_COLOR_MONO;
    CHECK(color_pattern_render(&s, 0, 42, reference) == ESP_OK);
    CHECK(color_pattern_render(&s, 2000, 42, pixels) == ESP_OK);
    CHECK(pixels[0].g > 0 && pixels[0].r == 0 && pixels[0].b == 0);
    for (int i = 1; i < 6; ++i) CHECK(memcmp(&pixels[0], &pixels[i], sizeof(expected)) == 0);
    CHECK(color_pattern_render(&s, 6000, 42, pixels) == ESP_OK);
    CHECK(memcmp(pixels, reference, sizeof(pixels)) == 0);

    for (int mode = 0; mode <= 1; ++mode) {
        s.color_mode = (light_color_mode_t)mode;
        s.shift_mode = LIGHT_SHIFT_RANDOM;
        unsigned changed = 0;
        bool independent = false, seeded = false;
        CHECK(color_pattern_render(&s, 0, 42, reference) == ESP_OK);
        for (uint64_t t = 42; t < 30000; t += 42) {
            CHECK(color_pattern_render(&s, t, 42, pixels) == ESP_OK);
            CHECK(color_pattern_render(&s, t, 43, other) == ESP_OK);
            if (memcmp(pixels, other, sizeof(pixels))) seeded = true;
            unsigned jumps = 0;
            for (unsigned i = 0; i < 6; ++i) {
                if (memcmp(&pixels[i], &reference[i], sizeof(expected))) jumps |= 1u << i;
            }
            if (jumps && jumps != 0x3f) independent = true;
            changed |= jumps;
            memcpy(reference, pixels, sizeof(pixels));
        }
        CHECK(changed == 0x3f && independent && seeded);
        uint64_t t = UINT64_C(5000000000);
        CHECK(color_pattern_render(&s, t, 42, reference) == ESP_OK);
        CHECK(color_pattern_render(&s, t + 99999, 42, pixels) == ESP_OK);
        CHECK(color_pattern_render(&s, t, 42, pixels) == ESP_OK);
        CHECK(memcmp(reference, pixels, sizeof(pixels)) == 0);
    }
    for (int mode = 0; mode <= 1; ++mode) for (int shift = 0; shift <= 2; ++shift) {
        s.color_mode = (light_color_mode_t)mode;
        s.shift_mode = (light_shift_mode_t)shift;
        s.on = false;
        CHECK(color_pattern_render(&s, 1234, 42, pixels) == ESP_OK);
        for (int i = 0; i < 6; ++i) CHECK(pixels[i].r == 0 && pixels[i].g == 0 && pixels[i].b == 0);
        s.on = true;
        s.brightness = 0;
        CHECK(color_pattern_render(&s, 1234, 42, pixels) == ESP_OK);
        for (int i = 0; i < 6; ++i) CHECK(pixels[i].r == 0 && pixels[i].g == 0 && pixels[i].b == 0);
        s.brightness = 0.3f;
    }
    s.shift_period_ms = UINT32_MAX;
    CHECK(color_pattern_validate(&s) == ESP_OK);
    CHECK(color_pattern_render(&s, UINT64_C(5000000000), 42, pixels) == ESP_OK);
    s.shift_period_ms = 1;
    CHECK(color_pattern_validate(&s) == ESP_OK);
    CHECK(color_pattern_render(&s, UINT64_C(5000000000), 42, pixels) == ESP_OK);
    s.shift_mode = (light_shift_mode_t)99;
    CHECK(color_pattern_validate(&s) == ESP_ERR_INVALID_ARG);
    s.shift_mode = LIGHT_SHIFT_STATIC;
    s.color_mode = (light_color_mode_t)99;
    CHECK(color_pattern_validate(&s) == ESP_ERR_INVALID_ARG);
    s.color_mode = LIGHT_COLOR_GRADIENT;
    s.gradient_end.x = NAN;
    CHECK(color_pattern_validate(&s) == ESP_ERR_INVALID_ARG);
    s.color_mode = LIGHT_COLOR_MONO; // Unused endpoint and static period are ignored.
    s.shift_period_ms = 0;
    CHECK(color_pattern_validate(&s) == ESP_OK);
    return 0;
}
