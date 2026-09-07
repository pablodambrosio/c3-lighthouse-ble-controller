# 001 - Group A lighting

Status: In progress  
Last updated: 2026-09-07

## Purpose

Current implementation note (2026-09-07): [increment 003](003-increment-simple-lighthouse.md) implements the user's revised first rotation: 1500 ms per revolution, six equal logical steps, one LED on at a time, no crossfade. It supersedes the smooth-animation timing, geometry and speed-control proposal below for the current release. [Increment 002](002-increment-xy-brightness.md) supersedes the former RGB settings. This document retains earlier requirements and hardware verification history; its full smooth-animation acceptance criteria remain pending.

Drive Group A's six downward-facing WS2812B LEDs and create the appearance of a rotating lighthouse light by smoothly moving illumination around the six perimeter positions, with no center LED.

## Scope

- Included: Group A driver, logical LED mapping, rotation animation, software brightness/color/speed settings, and a start/stop demonstration in the ESP-IDF application.
- Excluded: Group B implementation, BLE, persistent settings, deep sleep, MOSFET switching, and optical or mechanical changes to the lighthouse.

## Requirements

The initial diagnostic established the old five-LED cross mapping. On 2026-09-07, Group A changed to six perimeter LEDs with no center. The current implementation provides the `big_light` settings API and solid-light controls described below. Rotation and its acceptance criteria remain planned. The requested 45-degree spacing is provisional pending clarification: six consecutive positions at that spacing leave a 135-degree closing gap, while a uniform six-LED circle uses 60-degree spacing.

- R1: Use the [project pin assignments](../../README.md#led-group-pin-assignments): Group A data is D2/GPIO4. Reserve D3/GPIO5 for future power enable; do not drive it in this increment. Leave Group B's GPIO6/GPIO7 unused.
- R2: Control exactly six WS2812B RGB LEDs as one serial chain using Espressif's `led_strip` component with the RMT backend. Use one TX channel, no DMA, and leave the second TX channel available for Group B. Match the actual LEDs' color order and reset timing.
- R3: Keep physical chain indices separate from logical positions. Use the default mapping below, with a single mapping table that can accommodate different wiring without changing the animation.
- R4: Animate clockwise through positions 0 through 5 and back to 0. Crossfade adjacent perimeter LEDs. Confirm the angular spacing and clockwise mapping before implementing rotation; use the reference view below.
- R5: For rotation, default to a 4,000 ms revolution, a 20 ms frame interval, calibrated white `(255, 122, 135)`, and master brightness `64` on a `0-255` scale. The current solid-light startup uses brightness 255; the rotation default remains an initial tuning choice.
- R6: Derive the rotation phase from monotonic elapsed time. Skip obsolete frames after a delay rather than replaying them or accumulating timing drift. Apply a gamma 2.2 curve to each outer LED's fade weight, then master brightness and color scaling; clamp and round output channels to `0-255`.
- R7: Provide software controls for start, stop, color, brightness, and revolution period. Reject periods below 80 ms and invalid values without changing the current settings. Brightness zero produces black. Stop transmits black to all six LEDs and waits for completion; it does not disconnect their power.
- R8: Initialize and clear the strip before starting the default animation. Start begins at position 0. One task owns the strip and serializes setting changes and transfers. Log initialization or transfer failures and stop animation on failure; a failed transfer may leave the LEDs displaying their last frame.

## Implementation approach

### First step: fixed-color diagnostic and full-white check

The initial diagnostic used distinct colors at channel value 64 to establish the former cross mapping, which the user confirmed. A subsequent check set all LEDs to `(255, 255, 255)`. The user then calibrated white to `(255, 122, 135)`. The API startup now clears all six LEDs and submits solid calibrated white at brightness 255 with the following default mapping (physical clockwise order remains to be verified):

| Chain index | Logical position | Color | RGB |
| --- | --- | --- | --- |
| 0 | Position 0 | Calibrated white | `(255, 122, 135)` |
| 1 | Position 1 | Calibrated white | `(255, 122, 135)` |
| 2 | Position 2 | Calibrated white | `(255, 122, 135)` |
| 3 | Position 3 | Calibrated white | `(255, 122, 135)` |
| 4 | Position 4 | Calibrated white | `(255, 122, 135)` |
| 5 | Position 5 | Calibrated white | `(255, 122, 135)` |

`main/group_a.c` implements the internal strip driver and mapping. `main/lighting.c` exposes `lighting_init()` and `set_big_light()`, retains the calibrated white preset, and serializes hardware updates through a lighting task. `main/main.c` initializes the API and queues the startup settings. The LED driver dependency remains pinned to `espressif/led_strip` 3.0.3. Solid settings are transmitted on request, not periodically.

The preliminary color mapping check was confirmed for the old cross only. Runtime verification of the six-LED mapping, API, and calibrated-white startup remains pending. The individual-position diagnostic and full acceptance criteria below remain required for increment 001.

### Settings API

The [lighting API guide](../lighting-api.md) defines `big_light_settings_t`, `house_lights_settings_t`, stable effect IDs, and error handling. The `set_big_light(const big_light_settings_t *settings)` entry point copies a complete validated structure into a four-entry FIFO without blocking. `ESP_OK` means queued; the task logs transfer failures. Only solid lighting is currently supported; known future effects return `ESP_ERR_NOT_SUPPORTED`, and unknown effects return `ESP_ERR_INVALID_ARG`.

Color and brightness are independent. The calibrated white preset preserves the user's `(255, 122, 135)` proportions, with rounded channel scaling. `on = false` sends black without changing the caller's selected color, effect, or brightness. MOSFET outputs remain reserved until phase 3. Group B's public API is defined but explicitly unsupported until its driver increment; it does not touch hardware.

API verification must cover accepted settings being copied, FIFO ordering and queue-full rejection, brightness zero/full/half, off/on restoration, null pointers, invalid/unsupported effects, and initialization errors. Concurrency and visible light behavior still require target testing.

### Full increment

Use the existing ESP-IDF 6.0 project targeting `esp32c3`. Add a compatible, pinned dependency on `espressif/led_strip`. Keep the hardware driver, pure frame calculation, and application startup separate; file names can be chosen during implementation.

Configure the RMT backend for a 10 MHz resolution and a 48-symbol memory allocation for the Group A TX channel, with DMA disabled. The driver refills the hardware buffer as required. Do not increase the allocation in a way that consumes the channel reserved for Group B. Use the component's WS2812 model and verify its timing against the actual LED revision.

The reference view looks directly at the LEDs' emitting faces. Position 0 is the angular reference. Keep this view when wiring or checking rotation; viewing the board from its back reverses the apparent direction. The requested 45-degree layout is provisionally interpreted as:

```text
Position:       0    1    2     3     4     5
Angle (deg):   0   45   90   135   180   225
Closing gap from position 5 to position 0: 135 degrees
```

Default electrical order: GPIO4 through a suitable level buffer to position 0 DIN, then DOUT-to-DIN through positions 1, 2, 3, 4, and 5. Verify physical clockwise order and adjust the single mapping table if needed. The final DOUT remains unconnected. Power the LEDs directly for this increment with a common ground and local decoupling; GPIO5 remains reserved.

For elapsed time `t` and period `T`, derive the angular phase from `360 * ((t mod T) / T)`. Once spacing is confirmed, locate the adjacent positions around that angle and crossfade by the fraction of their angular interval. All other weights are zero; wrap position 5 to position 0. A uniform 60-degree layout reduces to `p = 6 * ((t mod T) / T)`. A 45-degree layout needs the larger closing interval represented explicitly to preserve angular speed. Apply the gamma curve to the weights before scaling by the selected RGB color and master brightness.

Update all pixel values before refreshing the strip once per frame. Use a task that sleeps between frame deadlines. Future BLE callbacks will submit commands to this owner rather than call the LED driver directly.

References: [Espressif LED strip component](https://components.espressif.com/components/espressif/led_strip/versions/3.0.3), [ESP-IDF 6.0 RMT documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c3/api-reference/peripherals/rmt.html), and [XIAO pin map](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/#pin-map).

## Acceptance criteria

- [ ] AC1: Firmware builds for ESP32-C3 with the pinned LED component and initializes Group A on GPIO4 without allocating Group B's channel or configuring GPIO5-7 as lighting outputs.
- [ ] AC2: A temporary diagnostic lights each of the six logical positions individually and displays red, green, and blue correctly, confirming wiring, mapping, and color order.
- [ ] AC3: At the default period, frame calculations select each position at `T * angle / 360` and position 0 again at 4,000 ms; midpoint frames blend adjacent positions. Confirm angles before finalizing these checks.
- [ ] AC4: The physical animation follows the reference direction and fades smoothly through the position 5-to-0 boundary. Observed revolution time agrees with the requested period within measurement and frame scheduling resolution.
- [ ] AC5: Color, brightness, and period controls work; zero brightness and stop clear all six LEDs. Invalid settings preserve the previous configuration. Restart begins at position 0.
- [ ] AC6: Simulating a delayed frame resumes at the position corresponding to elapsed time, without replaying missed frames. Gamma scaling remains bounded.
- [ ] AC7: A 10-minute hardware run at the default settings completes without unexpected color flashes, resets, or reported transfer errors. Record battery voltage and LED supply conditions.

## Verification

On 2026-09-07, the six-LED driver built successfully using the existing ESP-IDF 6.0.0 Ninja build for ESP32-C3. The resulting `build/lighthouse.bin` is 165,264 bytes and passes the partition size check. The strip length and solid-color mapping now cover indices 0 through 5. Firmware has not been flashed or verified on the six-LED hardware.

On 2026-09-05, the settings API and solid-light implementation built successfully for ESP32-C3 with ESP-IDF 6.0.0. The resulting binary is 165,264 bytes and passes the partition size check. Documentation links were checked. API runtime behavior, queue saturation/concurrency, and LED output after this change have not been tested on hardware; the build is not a substitute for those checks.

On 2026-09-05, the fixed-color diagnostic built successfully with ESP-IDF 6.0.0 for ESP32-C3 and `espressif/led_strip` 3.0.3. The generated `build/lighthouse.bin` is 164,592 bytes and passes the partition size check. The dependency resolution is recorded in `dependencies.lock`.

The initial build reported a nonfatal missing `ESP_ROM_ELF_DIR` warning while generating debugger symbol setup; compilation, linking, and binary generation succeeded. On 2026-09-05, the user confirmed the corrected mapping with Center red, North green, East blue, South yellow, and West magenta. The physical chain is West 0, North 1, Center 2, South 3, East 4. The diagnostic was then changed to full white at the user's request; hardware verification of full-white operation remains pending. All full-increment acceptance criteria remain pending.

During implementation, build with ESP-IDF, check the pure frame calculation at boundary/midpoint/delayed timestamps, and exercise invalid settings. On hardware, run the mapping/color diagnostic, check animation and stop/restart behavior, and record the 10-minute run. Use a logic analyzer or oscilloscope to confirm WS2812 pulse and reset timing for the actual LED revision if timing or color errors occur.

BLE coexistence testing belongs to phase 2: RMT without DMA depends on timely interrupt servicing, so successful standalone operation does not establish behavior during radio activity.

## Dependencies and open questions

- Depends on the conventions established in [increment 000](000-increment-documentation.md) and the pin assignments in the project README.
- Confirm the installed LEDs' exact WS2812B revision and physical chain order during assembly; adjust the timing configuration or mapping table as necessary.
- Confirm whether the six LEDs use 45-degree steps with a larger closing gap or uniform 60-degree spacing around a full circle before implementing rotation.
- Evaluate the rotation effect in the assembled lighthouse. Six downward-facing perimeter LEDs create a moving illumination pattern; a convincing projected beam depends on the model's openings and optics.
- GPIO5 is allocated, but the power-switch circuit and reset/sleep behavior remain phase 3 work and do not block this increment's directly powered prototype.
