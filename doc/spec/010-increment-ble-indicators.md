# 010 - BLE connection indicators

Status: In progress  
Last updated: 2026-09-09

## Purpose

Briefly show BLE connection state on Group A, then resume the configured lighting behavior.

## Scope

- Included: blue connection pulse, fast red disconnection flash, temporary output override, restoration, firmware tests and build.
- Excluded: new selectable effects, GATT fields, persisted indicator settings, Group B indication and hardware flashing.

## Requirements

- R1: A successful BLE connection triggers a 600 ms blue pulse on all six LEDs. Failed connection attempts do not trigger it.
- R2: A BLE disconnection triggers an immediate red flash fading out over 180 ms on all six LEDs. Durations are sampled at the existing 24 fps cadence.
- R3: Indicators use a peak raw PWM channel value of 128, independent of configured color/brightness, and appear even when Group A is off. Blue follows a half-sine envelope; red fades linearly.
- R4: Indicators must not change the lighting configuration, BLE readback or saved values. Afterward, show the latest accepted settings, including off/zero brightness. Effect and shift clocks continue underneath the override; do not restart animations merely because BLE connects or disconnects.
- R5: Only the lighting task may write the strip. BLE callbacks enqueue a nonblocking internal event; log queue-full errors. A newer processed indicator replaces the active indicator and restarts its duration. Settings commands remain accepted during indication.
- R6: Log strip-transfer failures and attempt normal rendering when indication fails. Preserve existing lighting error handling.

## Implementation approach

The existing four-entry lighting command queue carries an internal indicator marker distinct from public effect IDs. `lighting_ble_indicator()` submits it without invoking the settings or persistence path. The lighting task receives into a separate command variable so an event cannot replace the active configuration.

The owner task maintains indicator color and start time, wakes at its existing frame cadence even for static/off configurations, and temporarily renders a solid frame across Group A. It continues effect elapsed-time accounting and resumes normal rendering at the first frame after expiry. Successful connect and disconnect GAP callbacks request the corresponding indicator. No blocking delay runs in the BLE callback.

## Acceptance criteria

- [x] AC1: Lighting and BLE RISC-V/QEMU regression suites pass, including blue midpoint, solid restoration, red/off restoration, tick wrap and queue-full behavior.
- [x] AC2: Updated ESP32-C3 firmware builds and fits its partition.
- [ ] AC3: On hardware, connect/disconnect and verify perceived colors, timing and restoration for animated, static and off configurations.
- [ ] AC4: Verify rapid reconnects and settings writes during indication; verify BLE readback and persisted settings remain unaffected by the indicators themselves.

## Verification

2026-09-09: The `lighthouse` and `ble` RISC-V/QEMU suites passed. Indicator tests execute the real lighting task with simulated queue, clock and strip calls. Existing animation tests also pass, but they do not establish physical BLE/RMT behavior or visual timing. Hardware verification remains pending; no board was flashed.

## Dependencies and open questions

ESP-IDF 6.0 build passed on 2026-09-09: firmware size `0x6dff0` bytes, with 57% of the application partition free.

Extends [BLE control](008-increment-ble-lighting.md) while preserving [persistent settings](009-increment-persistent-settings.md). Indicator events share queue capacity with settings; under queue pressure an indication may be skipped with a warning. Actual pulse duration can extend to the next rendered frame.
