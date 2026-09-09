#pragma once
#include <math.h>
#include <stdint.h>

// One full dark -> peak -> dark cycle. Caller supplies a validated nonzero period.
// Gaussian width is 0.15 of a cycle. Subtract the endpoint tail so each
// breath reaches black; normalize so the midpoint retains full brightness.
static inline float breathing_level(uint64_t elapsed_ms, uint32_t period_ms)
{
    float phase = (float)(elapsed_ms % period_ms) / period_ms;
    const float endpoint = 0.00386592014f; // exp(-0.5 * (0.5 / 0.15)^2)
    float x = (phase - 0.5f) / 0.15f;
    float level = (expf(-0.5f * x * x) - endpoint) / (1.0f - endpoint);
    return fminf(1.0f, fmaxf(0.0f, level));
}
