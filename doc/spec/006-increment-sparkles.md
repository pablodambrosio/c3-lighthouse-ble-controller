# 006 - Sparkles

Status: Completed
Last updated: 2026-09-08

## Purpose

Add independent random sparkles to each of Group A's six LEDs.

## Scope

- Included: selected-color pulses, task integration, startup selection, documentation and automated checks.
- Excluded: Group B, BLE, power switching, board flashing and configurable sparkle speed.

## Requirements

- R1: Add `LIGHT_EFFECT_SPARKLES = 4`, preserving existing IDs. Select it at startup while retaining the current color and period values.
- R2: Each LED independently rises quickly and fades slowly to black, with pseudorandom timing, duration and peak intensity. Scale all channels together within the selected PWM ceiling.
- R3: Ignore `period_ms`. Use the existing 24 fps scheduler, immediate off/zero-output handling, mode switches, time-based frame skipping and stop-on-transfer-failure behavior.

## Implementation approach

`sparkles.c` renders from elapsed time and a per-update seed without mutable random state. Each LED has an independently offset 1200 ms window containing a randomly placed 250-650 ms pulse. Smoothstep shapes the quick rise and longer fade; the remainder of each window is dark. The lighting task shares its accumulated effect clock between candle and sparkles and writes complete frames through the existing chain mapping.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds and fits its application partition.
- [x] AC2: Renderer tests verify brightness limits, hue preservation, independent activity and dark gaps on every LED, seed variation and deterministic frame skipping beyond 32-bit milliseconds.
- [x] AC3: Owner-task regression tests verify sparkle rendering, off/restart, mode switches, delayed frames, tick wraparound and transfer failure.
- [x] AC4: Public API and README document the effect and startup selection.

## Verification

On 2026-09-08, the ESP-IDF 6.0 build passed and produced a 170752-byte binary, leaving 84% of the application partition free. After installing the missing Espressif RISC-V QEMU tool, all four suites passed: `sparkles`, `lighthouse`, `candle`, and `color`, using `python tests/test_light_color.py --suite <suite>`. The renderer tests sample one minute across four colors; task tests exercise startup, off/restart, solid/candle/lighthouse switches, delayed wakes across tick wraparound, and transfer failure. The test ELF linker reports an RWX segment from the existing test linker script. No hardware has been flashed; visual appearance requires a board check.

## Dependencies and open questions

Uses the existing XY color API, Group A frame driver and 24 fps scheduler. Pulse timing can be tuned after visual evaluation on the model.
