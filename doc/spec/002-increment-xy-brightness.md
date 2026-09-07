# 002 - XY and brightness lighting control

Status: Completed
Last updated: 2026-09-07

## Purpose

Represent requested colors independently of LED channel duty cycles so a future measured output map can compensate for the LEDs' color balance. Start with a standard sRGB/D65 map.

## Scope

- Included: CIE xy and relative luminance settings, standard sRGB input helper, raw PWM migration helper, output conversion, validation, driver integration and documentation.
- Excluded: measured device calibration, automatic ground/voltage compensation, BLE transport and hardware flashing.

## Requirements

- R1: `light_color_t` contains float x, y and brightness (relative luminance Y, 0-1); no separate RGB or brightness fields in `big_light_settings_t`.
- R2: Convert standard sRGB bytes through its transfer function and D65 XYZ matrix. Keep a separate linear PWM input helper to preserve existing device settings.
- R3: Map xy/brightness to linear LED PWM. Preserve chromaticity when requested luminance exceeds channel limits; reject nonfinite, invalid and out-of-gamut values before queueing.
- R4: Preserve startup PWM `(255,200,0)`, queue ownership, off behavior and effect IDs. Define `BIG_LIGHT_WHITE` as standard D65 reference white.

## Implementation approach

Keep color conversion and standard matrices in `main/light_color.c`. The lighting task handles xy/brightness settings and converts them before calling the Group A driver, which now receives final PWM bytes. RGB input characterization and output characterization are separate matrices so a later LED profile can replace the output mapping.

## Acceptance criteria

- [x] AC1: Public settings and callers use xy/brightness; firmware builds for ESP32-C3.
- [x] AC2: Tests pass for standard primary/white/yellow coordinates, sRGB gamma, raw PWM round trips, black, dimming, brightness limiting and invalid input rejection.
- [x] AC3: API documentation explains units, out-of-gamut handling and migration from raw RGB.

## Verification

ESP-IDF 6.0 firmware build passed on 2026-09-07. Conversion tests passed using the actual C code and ESP GCC math library under RISC-V QEMU: `python tests/test_light_color.py`. Verified standard primary/white/yellow coordinates, the sRGB transfer function, 14,097 exact raw PWM round trips (including startup), black, dimming, proportional brightness limiting, and rejection of invalid/nonfinite/out-of-gamut values without modifying output. No board was flashed; physical color accuracy, queue/off-on runtime behavior and actual LED calibration remain unverified on hardware.

## Dependencies and open questions

Depends on [Group A lighting](001-increment-group-a-lighting.md). The standard output map assumes linear PWM light output and standard primaries; actual LEDs can differ. Measured calibration and investigation of the suspected ground return remain separate work.
