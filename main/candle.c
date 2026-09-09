#include "candle.h"

#include <math.h>

static uint32_t hash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static float smooth(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

// Interpolate random targets rather than changing brightness abruptly each
// frame. Independent time scales make a flame that does not pulse in unison.
static float noise(uint64_t time_ms, uint32_t interval_ms, uint32_t seed)
{
    uint32_t knot = (uint32_t)(time_ms / interval_ms);
    float a = (hash(knot ^ seed) & 0xffffu) / 65535.0f;
    float b = (hash((knot + 1) ^ seed) & 0xffffu) / 65535.0f;
    float fraction = (time_ms % interval_ms) / (float)interval_ms;
    return a + (b - a) * smooth(fraction);
}

void candle_render_count(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color,
                   light_rgb_t *pixels, unsigned count)
{
    // Slow wandering of the flame leans the bright side around the perimeter;
    // faster small movements and local flicker move the shadows independently.
    float center = count * noise(elapsed_ms, 1900, seed ^ 0x173au) +
                   0.35f * (noise(elapsed_ms, 370, seed ^ 0xb493u) - 0.5f);
    if (center < 0.0f) center += count;
    if (center >= count) center -= count;

    float glow = 0.65f + 0.20f * noise(elapsed_ms, 310, seed ^ 0x847eu) +
                 0.10f * noise(elapsed_ms, 110, seed ^ 0x73b1u) +
                 0.05f * noise(elapsed_ms, 1300, seed ^ 0xc527u);

    for (unsigned i = 0; i < count; ++i) {
        float distance = fabsf(i - center);
        distance = fminf(distance, count - distance);
        float lobe = smooth(fmaxf(0.0f, 1.0f - distance / 2.0f));
        float local = noise(elapsed_ms, 170 + 23 * i, seed ^ hash(0x512du + i));
        float height = noise(elapsed_ms, 440, seed ^ hash(0xab37u + i));
        // A low base keeps the flame alive while a broad bright patch wanders.
        float level = glow * (0.14f + 0.14f * local + lobe * (0.50f + 0.18f * height));
        pixels[i] = (light_rgb_t){
            (uint8_t)lroundf(color.r * level),
            (uint8_t)lroundf(color.g * level),
            (uint8_t)lroundf(color.b * level),
        };
    }
}

void candle_render(uint64_t elapsed_ms, uint32_t seed, light_rgb_t color, light_rgb_t pixels[GROUP_A_LED_COUNT])
{
    candle_render_count(elapsed_ms, seed, color, pixels, GROUP_A_LED_COUNT);
}
