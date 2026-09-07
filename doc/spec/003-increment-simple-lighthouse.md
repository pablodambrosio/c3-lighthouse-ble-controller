# 003 - Simple lighthouse rotation

Status: Completed  
Last updated: 2026-09-07

## Purpose

The on/off transition described here is superseded by [increment 004](004-increment-lighthouse-crossfade.md), which adds a 10 fps crossfade. The settings API, period validation and control behavior remain in use.

Start the lighthouse effect with a configurable logical 360-degree revolution, lighting one of six LEDs at a time with on/off transitions. Startup uses 1.5 seconds.

## Scope

- Included: stepping with a `period_ms` setting, owner-task scheduling, single-position driver frames, startup selection and existing color/on/off controls.
- Excluded: fading, physical LED rearrangement, hardware flashing, BLE and Group B.

## Requirements

- R1: Accept `LIGHT_EFFECT_LIGHT_HOUSE` and select it at startup, preserving the user's raw `(255,150,30)` color.
- R2: Cycle logical positions 0,1,2,3,4,5,0 over `period_ms`, defaulting to 1500 ms (250 ms per position). Share the rounded-up tick period across six positions without accumulating rounding error. Reject periods shorter than six ticks, or beyond half the tick counter range. SOLID ignores the period. Set the other five LEDs to black in each frame and refresh once.
- R3: Let settings interrupt the animation wait. Off or zero output clears the strip and suspends stepping; solid mode lights all six. Each new settings request starts a fresh revolution at position 0.
- R4: Derive steps from elapsed RTOS ticks, skip missed positions and handle tick wraparound. Stop automatic updates after a driver failure until another request arrives.

## Implementation approach

`lighting.c` retains sole strip ownership and waits on the existing queue until the next step deadline. `group_a_set_single()` builds a full frame using the existing chain mapping. The default mapping assumes positions 0-5 follow physical perimeter order; a clockwise physical rotation still needs visual confirmation. The startup 1500 ms period is 150 ticks at the configured 100 Hz RTOS tick rate. Period changes restart at position 0.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds with lighthouse startup and unchanged selected color.
- [x] AC2: Task tests cover all six positions and wrap at 1500 ms, immediate off/restart, solid and zero-output changes, delayed wakes, tick wraparound, transfer failure and setter validation.
- [x] AC3: Driver fills all six pixels with only the selected position nonzero, using one refresh per frame; verified by source review.
- [x] AC4: Public API and README describe the step effect and its period limits.

## Verification

The configurable-period update built successfully for ESP32-C3 (167,696-byte binary, partition check passed). It also passed RISC-V task tests for 1000 ms rotations with uneven tick allocation, live period changes, 61 ms rounded to 70 ms, rejection of zero/subminimum periods, SOLID ignoring the period, and overflow-safe conversion of `UINT32_MAX` milliseconds.

2026-09-07: ESP-IDF 6.0 build passed for ESP32-C3 and produced `build/lighthouse.bin` (167,456 bytes), passing the partition-size check. `python tests/test_light_color.py --suite lighthouse` passed under RISC-V QEMU, executing the actual lighting task with simulated queue, tick clock and strip calls. Source review confirmed the driver clears all nonselected positions before refreshing. No board was flashed. Physical order/direction, visible timing and RMT operation remain unverified on hardware.

## Dependencies and open questions

Uses [Group A driver](001-increment-group-a-lighting.md) and [XY/brightness colors](002-increment-xy-brightness.md). This simple step effect supersedes the earlier smooth 4-second rotation proposal for now. Physical geometry can affect the apparent angular spacing of each step.
