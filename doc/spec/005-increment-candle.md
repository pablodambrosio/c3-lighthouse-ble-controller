# 005 - Candle with moving illumination

Status: Completed
Last updated: 2026-09-07

## Purpose

Use six separately controlled light sources to create moving candle-like illumination and shadows. Apply the user's color and brightness settings so different flame colors are possible.

## Scope

- Included: spatial flame model, smooth pseudorandom flicker, 24 fps scheduling, selected-color scaling, startup selection, driver frames and tests.
- Excluded: physical flame simulation, automatic color changes, configurable flame parameters, hardware flashing and optical calibration.

## Requirements

- R1: Implement `LIGHT_EFFECT_CANDLE` and select it at startup, preserving the user's current color and period values. Candle ignores `period_ms`.
- R2: Combine a low base glow with a wandering broad bright region and independent local flicker, interpolating all changes smoothly.
- R3: Scale the selected linear RGB channels together at every position. Each channel stays within its selected level; zero channels stay zero.
- R4: Share the 24 fps scheduler. Support immediate off/zero output and mode switches, fresh variations on updates, tick wraparound, frame skipping and stop-on-transfer-failure.

## Implementation approach

`candle.c` is a pure function of elapsed milliseconds, a seed and selected LED RGB. Smooth interpolated noise at different time scales controls a circular bright region, overall glow and local variations. `lighting.c` owns timing and creates a new seed for each candle settings update. `group_a_set_frame()` writes all logical positions and refreshes once.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds and fits the application partition.
- [x] AC2: RISC-V tests pass for output limits, hue preservation within byte rounding, temporal continuity, spatial movement, deterministic frame skipping, seed differences and a 32-bit millisecond boundary.
- [x] AC3: Owner-task tests pass for candle rendering, off/restart, zero output, mode changes, delayed frames, tick wrap and transfer failures; lighthouse regression tests also pass.
- [x] AC4: API documentation describes selected-color behavior and ignored period; driver source review confirms all positions use the mapping and one refresh.

## Verification

`python tests/test_light_color.py --suite candle` and `--suite lighthouse` passed under RISC-V QEMU on 2026-09-07. Pure tests sample one minute for seven input colors, verify all six positions become the brightest, and bound changes between adjacent samples. Task tests use simulated queue, clock and strip calls. A clean ESP-IDF 6.0 build passed in `build/candle-verification` with a copied sdkconfig, producing a 169,712-byte binary and passing the partition-size check. The existing build directory had a missing intermediate library and malformed generated project metadata, so verification used a separate directory. No board was flashed; perceived realism and actual shadow movement require visual tuning on the model.

## Dependencies and open questions

Uses the [XY color API](002-increment-xy-brightness.md) and [24 fps animation scheduler](004-increment-lighthouse-crossfade.md). Hardware geometry and light diffusion will determine the visible result.
