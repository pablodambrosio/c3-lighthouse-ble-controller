# 011 - Four-LED Group B lighting

Status: In progress  
Last updated: 2026-09-09

## Purpose

Implement independent window lighting using four WS2812B LEDs on GPIO6 (D4), supporting Solid, Candle, Sparkles and Breathing. Lighthouse remains exclusive to Group A.

## Scope

- Included: four-LED RMT driver, independent command queue/task and animation clocks, all color/shift controls, BLE characteristics, NVS persistence and HTML group selection.
- Excluded: MOSFET power switching, Group B Lighthouse, Group B connection indicators, physical flashing and calibration.

## Requirements

- R1: Render exactly four LEDs in chain order at 24 fps for animated settings. Start off by default; load valid saved Group B settings independently of Group A.
- R2: Support MONO/GRADIENT, STATIC/CYCLE/RANDOM, shared brightness, effect period and shift period. Static gradient positions are A, midpoint, B, midpoint; the four positions cover the whole gradient loop.
- R3: Reject Lighthouse for Group B, including stored records. Preserve Group A effects and BLE indicators.
- R4: Give each group its own settings queue, driver, clocks and persistence key. A write to one group's GATT controls must not patch the other group's state.
- R5: Extend GATT without renumbering existing Group A characteristics. Keep 000b as Group B on; add 000c-0013 for its remaining settings and 0014 for capabilities. Group B effect mask is 0x35; supported-group mask is 0x03.
- R6: Allow HTML selection of either group. Read settings on switching, discard unsubmitted edits, disable unsupported effects, and use a four-LED preview for Group B.

## Implementation approach

`group_b.c` owns the GPIO6 strip and a four-entry settings queue. Its renderer shares count-aware color, candle and sparkle functions with Group A; six-LED wrappers retain the existing Group A interface. Breathing applies one common cosine intensity envelope to all four pixels.

Storage uses independent worker contexts and `lighting/group_a` and `lighting/group_b` records with the existing versioned format and two-second quiet interval. Existing Group A records remain readable. Group B defaults in `main.c` are off, Solid, Mono/Static, orange, brightness 1, 4000 ms effect period and 5000 ms shift period. GPIO7 remains reserved and unused.

## Acceptance criteria

- [x] AC1: Firmware builds and fits the application partition.
- [x] AC2: Existing shared-renderer and Group A tests pass; BLE tests cover independent Group B writes and Lighthouse rejection.
- [ ] AC3: Verify all four LEDs, color order and each effect on hardware while Group A is active.
- [ ] AC4: Verify HTML switching, all BLE fields, independent save/reboot restoration, and concurrent radio/RMT operation on hardware.

## Verification

ESP-IDF 6.0 build and RISC-V/QEMU regression checks pass. Tests use simulated driver/queue calls and do not establish physical RMT operation. Hardware and browser-to-board checks remain pending; no firmware was flashed.

## Dependencies and open questions

Extends [BLE control](008-increment-ble-lighting.md) and [persistent settings](009-increment-persistent-settings.md). Uses the second RMT transmit channel; verify simultaneous strip operation on ESP32-C3. BLE indicators remain on Group A only. Four LEDs supersede the earlier five-window-LED plan for this increment.
