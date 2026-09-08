# 001 - Group A lighting

Status: In progress
Last updated: 2026-09-08
Implementation: Feature complete
Hardware verification: Pending

## Purpose

Drive Group A's six downward-facing WS2812B LEDs and provide the complete lighting-control feature set. On 2026-09-08, the user agreed that the Group A light-control increments are covered. No further Group A features are planned for this phase.

The overall increment remains In progress only because the hardware acceptance checks below are outstanding. Feature completion does not establish physical mapping, visual quality, or long-run stability.

## Scope

- Implemented: six-LED Group A driver, logical mapping, effects, shared brightness, color modes, color shifting, validated settings and serialized frame updates.
- Pending verification: six-LED mapping/color order, observed animation and control behavior, and a sustained hardware run.
- Outside this increment: Group B window lighting, BLE, persistence, deep sleep, MOSFET switching and optical/mechanical changes.

## Requirements

- R1: Drive six WS2812B LEDs on D2/GPIO4 using the pinned `espressif/led_strip` 3.0.3 RMT backend. Use one TX channel with DMA disabled. GPIO5 remains reserved for power enable; GPIO6/7 remain unused until Group B and power management work.
- R2: Keep logical positions 0-5 separate from chain indices through `chain_index` in `main/group_a.c`. The current mapping follows serial chain order; verify physical direction on the model.
- R3: Provide SOLID, LIGHT_HOUSE, CANDLE and SPARKLES effects. Lighthouse crossfades adjacent positions at an average 24 fps using `period_ms` per logical revolution. Candle and sparkles modulate per-LED intensity.
- R4: Store `color` and `gradient_end` as x/y-only `light_xy_t` values, with one separate shared `brightness` value from 0.0 to 1.0. Support MONO and GRADIENT color modes with STATIC, CYCLE and independent RANDOM shifting, timed by `shift_period_ms`.
- R5: Validate settings before copying them into the four-entry FIFO. One lighting task owns transfers. Off or zero brightness sends black; settings updates restart timing. Skip obsolete animation frames after delays. Transfer errors suspend animation until another request.
- R6: Keep hardware acceptance separate from software checks. Record real observations before closing the outstanding criteria.

## Implementation approach

The current contracts are defined in the [lighting API guide](../lighting-api.md) and these completed increments:

| Increment | Delivered behavior |
| --- | --- |
| [002 - XY and brightness](002-increment-xy-brightness.md) | Color conversion and luminance validation; the later brightness refactor is recorded in increment 007. |
| [003 - Simple lighthouse rotation](003-increment-simple-lighthouse.md) | Initial stepped rotation, subsequently replaced by increment 004. |
| [004 - Lighthouse crossfade](004-increment-lighthouse-crossfade.md) | Time-based adjacent-LED crossfades at 24 fps. |
| [005 - Candle](005-increment-candle.md) | Moving illumination and local flicker. |
| [006 - Sparkles](006-increment-sparkles.md) | Independent flashes and fades. |
| [007 - Color modes and shifting](007-increment-color-modes.md) | MONO/GRADIENT, STATIC/CYCLE/RANDOM and separate shared brightness. |

`main/group_a.c` owns the driver and mapping. `main/lighting.c` validates and queues settings and schedules frames. `main/color_pattern.c`, `main/candle.c` and `main/sparkles.c` calculate colors and intensities. `main/main.c` selects startup settings.

The earlier 4,000 ms/50 fps rotation proposal, gamma-weighted fades, calibrated-white startup and provisional 45-degree geometry are superseded by the linked increments. The current animation uses six equal logical positions around a revolution; physical placement and the resulting visual effect still require checking on the model.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds with the pinned LED component and fits the application partition. Source inspection confirms six LEDs on GPIO4, one RMT TX channel without DMA, and GPIO5-7 unused as lighting outputs.
- [x] AC2: Software controls cover all four effects, both color modes, all three shift modes, separate shared brightness, on/off and applicable timing settings.
- [x] AC3: Automated tests verify color conversion, crossfades, candle and sparkle rendering, color shifting, settings validation, off/zero brightness, mode transitions, delayed frames, tick wrap and transfer-failure suspension.
- [ ] AC4: A hardware diagnostic identifies all six logical positions and displays red, green and blue correctly, confirming mapping and color order.
- [ ] AC5: On the assembled model, verify rotation direction and timing, the 5-to-0 transition, candle/sparkle appearance, gradients and shifting, and brightness/off/on behavior.
- [ ] AC6: A 10-minute hardware run completes without unexpected flashes, resets or transfer errors. Record battery voltage, LED supply conditions and tested settings.

## Verification

On 2026-09-08, the Group A feature set was agreed complete. The latest ESP-IDF 6.0 build passed, producing a 173,584-byte binary with 83% of the application partition free. All five RISC-V/QEMU suites passed: `color`, `lighthouse`, `candle`, `sparkles` and `pattern`. These include 14,097 PWM round trips and task tests for color/shift combinations with every effect. The test linker retains its existing RWX segment warning.

No new board verification was performed during these software increments. The six-LED hardware acceptance criteria remain unchecked. Automated queue, clock and driver spies do not establish physical RMT timing or hardware concurrency behavior.

Historical verification: on 2026-09-05, the user confirmed the former five-LED cross diagnostic (West 0, North 1, Center 2, South 3, East 4). That result does not verify the current six-position mapping. On 2026-09-07, the six-LED driver built successfully; hardware mapping verification remained pending.

## Dependencies and remaining work

- Follow the [project pin assignments](../../README.md#led-group-pin-assignments) and verify the installed WS2812B revision, physical chain order and visible result during assembly.
- Group B window lighting is the remaining phase 1 implementation work. `set_house_lights()` is still a stub returning `ESP_ERR_NOT_SUPPORTED` for non-null settings.
- Phase 1 remains In progress until Group B is implemented and the outstanding hardware checks are recorded.
- BLE and radio-coexistence verification belong to phase 2. Deep sleep, MOSFET switching and power sequencing belong to phase 3.
