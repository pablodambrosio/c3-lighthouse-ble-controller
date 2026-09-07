#include <reent.h>
#include <stdlib.h>
#include "candle.h"

static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    const light_rgb_t colors[] = {{255, 80, 20}, {0, 0, 255}, {0, 255, 0},
                                  {255, 0, 255}, {128, 128, 128}, {1, 1, 1}, {0}};
    light_rgb_t pixels[GROUP_A_LED_COUNT], previous[GROUP_A_LED_COUNT] = {0};
    unsigned visited = 0;
    int smallest_total = 6 * 255, largest_total = 0;
    for (unsigned c = 0; c < sizeof(colors) / sizeof(colors[0]); ++c) {
        for (uint64_t t = 0; t < 60000; t += 42) {
            candle_render(t, 0xabcdu, colors[c], pixels);
            int total = 0, brightest = 0;
            for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
                CHECK(pixels[i].r <= colors[c].r && pixels[i].g <= colors[c].g &&
                      pixels[i].b <= colors[c].b);
                // Same gain for all channels, allowing only byte rounding error.
                CHECK(abs(pixels[i].r * colors[c].g - pixels[i].g * colors[c].r) <=
                      (colors[c].r + colors[c].g + 1) / 2);
                CHECK(abs(pixels[i].r * colors[c].b - pixels[i].b * colors[c].r) <=
                      (colors[c].r + colors[c].b + 1) / 2);
                if (t != 0) CHECK(abs((int)pixels[i].r - previous[i].r) <= 70);
                previous[i] = pixels[i];
                total += pixels[i].r;
                if (pixels[i].r > pixels[brightest].r) brightest = i;
            }
            if (c == 0) {
                visited |= 1u << brightest;
                if (total < smallest_total) smallest_total = total;
                if (total > largest_total) largest_total = total;
                CHECK(total > 0);
            }
        }
    }
    // Bright area must move around the ring, rather than merely global blinking.
    CHECK(visited == 0x3fu);
    CHECK(largest_total - smallest_total > 50);

    light_rgb_t reference[GROUP_A_LED_COUNT], other[GROUP_A_LED_COUNT];
    candle_render(123456, 42, colors[0], reference);
    candle_render(987654, 42, colors[0], other); // Skipped frames do not alter state.
    candle_render(123456, 42, colors[0], pixels);
    bool different = false;
    candle_render(123456, 43, colors[0], other);
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        CHECK(pixels[i].r == reference[i].r && pixels[i].g == reference[i].g &&
              pixels[i].b == reference[i].b);
        if (other[i].r != pixels[i].r) different = true;
    }
    CHECK(different);
    // The render clock must not wrap at the 32-bit millisecond boundary.
    candle_render(UINT32_MAX, 42, colors[0], reference);
    candle_render((uint64_t)UINT32_MAX + 42, 42, colors[0], pixels);
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        CHECK(abs((int)pixels[i].r - reference[i].r) <= 70);
    }
    return 0;
}
