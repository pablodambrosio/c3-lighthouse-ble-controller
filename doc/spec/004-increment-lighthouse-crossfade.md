# 004 - Lighthouse crossfade

Status: Completed  
Last updated: 2026-09-07

## Purpose

Interpolate illumination between neighbouring LEDs at 24 frames per second, retaining the configured full-revolution period and the user's 10000 ms startup setting.

## Scope

- Included: linear crossfade, 24 fps frame scheduling, wraparound, driver pair frames and numerical/task tests.
- Excluded: variable frame rate, gamma fade curves, device color calibration and hardware flashing.

## Requirements

- R1: Derive the outgoing LED and fractional transition to the next LED from elapsed rotation phase. Wrap position 5 to 0.
- R2: Split each linear PWM channel between those LEDs, preserving its integer sum exactly. Clear all other LEDs before one strip refresh.
- R3: Render at 24 fps average without cumulative drift; skip obsolete frames after delays. Settings changes interrupt the wait and restart at position 0. Off/zero-output, solid mode and transfer-failure behavior remain supported.
- R4: Retain period validation and startup color/period. Document sampling limits for short periods.

## Implementation approach

`lighthouse_frame()` in `lighting.c` calculates position and complementary channel values with wide integer arithmetic. The owner maintains separate frame and revolution timing. `group_a_set_pair()` maps two logical neighbours to the chain and clears the remaining pixels.

## Acceptance criteria

- [x] AC1: ESP32-C3 build and partition-size check pass.
- [x] AC2: RISC-V tests verify 24 fps scheduling, example blend values, midpoint/endpoints, last-to-first transition and exact channel conservation across a 500-tick revolution.
- [x] AC3: Task tests pass for delayed frames, clock wrap, live period changes, off/restart, solid/zero output and transfer failure.
- [x] AC4: Driver source review confirms adjacent mapping, wraparound, clearing other pixels and one refresh per frame; documentation reflects the new effect.

## Verification

The 24 fps update passed a five-second cadence test: exactly 120 frame intervals, including tick wraparound, with 50/40/40/40/40/40 ms intervals at 100 Hz and no accumulated drift. Existing interpolation, delayed-frame and control tests also passed.

2026-09-07: ESP-IDF 6.0 build passed (167,936-byte binary). `python tests/test_light_color.py --suite lighthouse` passed using the production task and interpolation code with simulated clock, queue and strip calls under RISC-V QEMU. The driver pair frame was reviewed. No board was flashed; actual timing, physical direction and perceived smoothness remain unverified on hardware.

## Dependencies and open questions

Extends [increment 003](003-increment-simple-lighthouse.md). Very short periods can skip LEDs or appear stationary when sampled at 24 fps. Actual LED brightness balance remains dependent on device calibration and power conditions.
