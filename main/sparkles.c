#include "sparkles.h"

#include <math.h>

static uint32_t hash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

void sparkles_render_count(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color,
                     light_rgb_t *pixels, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        uint32_t local_seed = hash(seed ^ (0x9e3779b9u * (i + 1)));
        uint64_t time = elapsed_ms + local_seed % 1200;
        uint32_t random = hash((uint32_t)(time / 1200) ^ local_seed);
        unsigned duration = 250 + random % 401;
        unsigned start = (random >> 10) % (1200 - duration);
        unsigned phase = time % 1200;
        float level = 0.0f;
        if (phase >= start && phase < start + duration) {
            float progress = (phase - start) / (float)duration;
            // Quick rise, longer fade; zero slope at the peak and both ends.
            float envelope = progress < 0.25f ? progress * 4.0f :
                             (1.0f - progress) / 0.75f;
            level = envelope * envelope * (3.0f - 2.0f * envelope);
            level *= 0.65f + 0.35f * ((random >> 20) & 0xfffu) / 4095.0f;
        }
        pixels[i] = (light_rgb_t){
            (uint8_t)lroundf(color.r * level),
            (uint8_t)lroundf(color.g * level),
            (uint8_t)lroundf(color.b * level),
        };
    }
}

void sparkles_render(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color, light_rgb_t pixels[GROUP_A_LED_COUNT])
{
    sparkles_render_count(elapsed_ms, seed, color, pixels, GROUP_A_LED_COUNT);
}
