# 007 - Color modes and shifting

Status: Completed
Last updated: 2026-09-08

## Purpose

Choose the colors of individual LEDs independently from their lighting effect,
and optionally cycle or randomly change those colors over time.

## Scope

- Included: color modes, shift modes, shift timing, composition with all Group A effects, validation, documentation and automated verification.
- Excluded: BLE and Group B implementation, hardware flashing.

## Requirements

- R1: A single-color mode gives all LEDs the selected chromaticity when static.
- R2: A gradient mode distributes colors across LEDs using linear interpolation of CIE x,y coordinates.
- R3: Static shifting holds colors; cycling moves smoothly through hues or the gradient; random shifting chooses an independent hue or gradient point for each LED.
- R4: `shift_period_ms` controls color shifting independently of lighthouse rotation timing.
- R5: Existing zero-initialized settings retain single-color, static behavior. Color modes compose with solid, lighthouse, candle and sparkles effects.

## Implementation approach

Generate per-LED colors before applying the lighting effect's intensity. Keep
color shifting and effect timing separate while sharing the 24 fps scheduler.
Interpolate gradient chromaticities before converting to linear LED PWM.

## Acceptance criteria

- [x] AC1: Define gradient endpoints, cyclic wrapping and random timing precisely.
- [x] AC2: Verify all six color/shift combinations, validation, period boundaries and deterministic time-based rendering.
- [x] AC3: Verify composition with existing effects, immediate off/zero brightness, mode switches and tick wraparound.
- [x] AC4: Firmware builds and existing regression suites pass; API documentation includes examples.

## Verification

On 2026-09-08, all five RISC-V/QEMU suites passed: pattern, lighthouse, candle, sparkles and color. The expanded task suite covers every color/shift combination with all four effects, invalid settings without queue replacement, off/zero brightness, delayed frames across tick wrap, mode switches and transfer-failure suspension. The ESP-IDF 6.0 firmware build passed and fits the application partition with 83% free. QEMU tests retain the existing test-linker RWX segment warning. No board was flashed; appearance still needs hardware evaluation.

## Dependencies and open questions

Implementation assumptions: the gradient has a second explicit x,y endpoint and rotates around the ring. It follows A -> B -> A with linear interpolation and a continuous seam. Names are MONO / GRADIENT and STATIC / CYCLE / RANDOM. MONO avoids confusion with the SOLID lighting effect.

CYCLE uses a full hue turn or ring rotation per shift period; RANDOM staggers each LED's jump time and samples independently once per period. MONO cycling retains linear-RGB saturation; an achromatic base starts at red with full saturation. Both endpoints share brightness. Nonstatic periods accept 1 through UINT32_MAX ms; short periods may be undersampled at 24 fps.


Brightness refactor (2026-09-08): both endpoints now use light_xy_t, with one explicit brightness field on big_light_settings_t. Conversion accepts x/y and brightness separately; matching RGB/PWM luminance helpers preserve input output levels. All five regression suites passed after the refactor, including 14,097 PWM round trips. Startup retains the current GRADIENT + RANDOM configuration and original luminance.
