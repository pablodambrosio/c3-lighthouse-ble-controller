#include <reent.h>
#include <stdlib.h>
#include <string.h>
#include "sparkles.h"

static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    const light_rgb_t colors[] = {{255, 80, 20}, {0, 0, 255}, {1, 1, 1}, {0}};
    light_rgb_t pixels[GROUP_A_LED_COUNT], reference[GROUP_A_LED_COUNT];
    unsigned lit = 0, dark = 0;
    bool independent = false, changed_seed = false;
    for (unsigned c = 0; c < sizeof(colors) / sizeof(colors[0]); ++c) {
        for (uint64_t t = 0; t < 60000; t += 42) {
            sparkles_render(t, 42, colors[c], pixels);
            for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
                CHECK(pixels[i].r <= colors[c].r && pixels[i].g <= colors[c].g &&
                      pixels[i].b <= colors[c].b);
                CHECK(abs(pixels[i].r * colors[c].g - pixels[i].g * colors[c].r) <=
                      (colors[c].r + colors[c].g + 1) / 2);
                CHECK(abs(pixels[i].r * colors[c].b - pixels[i].b * colors[c].r) <=
                      (colors[c].r + colors[c].b + 1) / 2);
                if (c == 0) {
                    if (pixels[i].r > 100) lit |= 1u << i;
                    if (pixels[i].r == 0) dark |= 1u << i;
                    if (pixels[i].r != pixels[0].r) independent = true;
                }
            }
        }
    }
    CHECK(lit == 0x3f && dark == 0x3f && independent);
    for (uint64_t t = UINT32_MAX; t < (uint64_t)UINT32_MAX + 5000; t += 42) {
        sparkles_render(t, 42, colors[0], reference);
        sparkles_render(t + 99999, 42, colors[0], pixels);
        sparkles_render(t, 42, colors[0], pixels);
        CHECK(memcmp(pixels, reference, sizeof(pixels)) == 0);
        sparkles_render(t, 43, colors[0], pixels);
        if (memcmp(pixels, reference, sizeof(pixels)) != 0) changed_seed = true;
    }
    CHECK(changed_seed);
    return 0;
}
