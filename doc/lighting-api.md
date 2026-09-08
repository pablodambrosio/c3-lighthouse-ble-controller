# Lighting control API

Status: In progress  
Last updated: 2026-09-08

The public firmware interface is [main/lighting.h](../main/lighting.h). Group A is named `big_light`; Group B is named `house_lights`. The application and BLE callbacks use this interface; the lighting task owns LED updates.

## Settings

`big_light_settings_t` contains:

| Field | Type | Meaning |
| --- | --- | --- |
| `on` | `bool` | Requested on/off state. False displays black while retaining the caller's color and brightness settings. |
| `effect` | `light_effect_t` | Effect ID from the table below. |
| `period_ms` | `uint32_t` | Full lighthouse revolution time in milliseconds; ignored for SOLID, CANDLE and SPARKLES. Set explicitly for lighthouse mode, including when off. |
| `color` | `light_xy_t` | Base CIE x,y chromaticity only. |
| `brightness` | `float` | Shared relative luminance Y for both endpoints, 0.0 to 1.0. Must be set explicitly; zero is black. |
| `color_mode` | `light_color_mode_t` | `LIGHT_COLOR_MONO` (0) or `LIGHT_COLOR_GRADIENT` (1). |
| `shift_mode` | `light_shift_mode_t` | `LIGHT_SHIFT_STATIC` (0), `LIGHT_SHIFT_CYCLE` (1), or `LIGHT_SHIFT_RANDOM` (2). |
| `gradient_end` | `light_xy_t` | Second gradient x,y endpoint; shares `brightness`. Ignored in MONO. |
| `shift_period_ms` | `uint32_t` | Full color cycle or each LED's random jump interval. Must be nonzero unless STATIC. |

`house_lights_settings_t` currently contains only `bool on`. `lighting_settings_t` groups both settings structures under the fields `big_light` and `house_lights`; it is a convenience data type, not an atomic update API.

| Effect constant | ID | Current support | Intended behavior |
| --- | --- | --- | --- |
| `LIGHT_EFFECT_SOLID` | 0 | Implemented | All six LEDs display the chosen color at the selected brightness. |
| `LIGHT_EFFECT_LIGHT_HOUSE` | 1 | Implemented | Linear crossfade between adjacent LEDs at 24 fps; `period_ms` per revolution. |
| `LIGHT_EFFECT_CANDLE` | 2 | Implemented | Wandering bright region across six LEDs with smooth local flicker at 24 fps, using the selected color. |
| `LIGHT_EFFECT_SPARKLES` | 4 | Implemented | Independent random flashes with a quick rise and slower fade at 24 fps. Selected at startup. |

## Color modes and shifting

Color selection is independent of the lighting effect. The effect controls intensity; the color mode and shift mode supply each LED's color before that intensity is applied. `MONO` avoids confusion with the existing SOLID lighting effect; `CYCLE` describes the smooth looping behavior more precisely than DYNAMIC. Zero-initialized new fields preserve MONO + STATIC.

| Color mode | STATIC | CYCLE | RANDOM |
| --- | --- | --- | --- |
| MONO | All LEDs use `color`. | All LEDs sweep the hue wheel together. | Each LED independently chooses a hue and holds it until its next jump. |
| GRADIENT | Fixed gradient around the ring. | Gradient rotates around the ring in increasing position order. | Each LED independently chooses a point along the gradient and holds it until its next jump. |

A gradient requires two endpoints: `color.x/y` and `gradient_end.x/y`. Interpolate x and y linearly, then convert to PWM using the shared `brightness`. To join smoothly around the ring, the gradient runs A -> B -> A. With six LEDs, its static interpolation fractions are 0, 1/3, 2/3, 1, 2/3, 1/3. Both endpoints must be inside the supported sRGB triangle, including when off or brightness is zero. MONO ignores `gradient_end`.

For CYCLE, `shift_period_ms` is one full hue revolution or one full gradient rotation. MONO uses a linear-RGB hue wheel starting at the selected hue and retains its saturation; an achromatic starting color uses full saturation and starts at red. Shared luminance remains the requested value, subject to the existing gamut/output ceiling. RANDOM samples hues or gradient positions independently, with staggered per-LED jump times; each LED jumps once per period, without a fade. MONO + RANDOM intentionally allows different LED colors.

STATIC ignores `shift_period_ms`, including zero. Other shift modes accept 1 through UINT32_MAX milliseconds. The 24 fps scheduler may skip changes at short periods; settings updates restart both clocks and random updates start a new variation. Color shifting also animates the SOLID lighting effect. Off and zero brightness clear the LEDs and suspend animation. `period_ms` remains exclusively the lighthouse rotation period.

```c
big_light.color_mode = LIGHT_COLOR_GRADIENT;
big_light.color = light_color_from_rgb(255, 100, 0);
big_light.gradient_end = light_color_from_rgb(0, 0, 255);
big_light.brightness = 0.3f;
big_light.shift_mode = LIGHT_SHIFT_CYCLE;
big_light.shift_period_ms = 10000;
esp_err_t err = set_big_light(&big_light);
```

All four lighting effects support these settings. Lighthouse intensity still crossfades between adjacent positions, but their colors can differ; combined RGB channel totals are conserved only when the two colors match. Candle and sparkles scale generated colors using their per-LED intensity envelopes. Startup uses the settings in `main/main.c`, currently sparkles with GRADIENT + RANDOM.

Run `python tests/test_light_color.py --suite pattern` for color generation and `--suite lighthouse` for effect composition and task transitions. See [increment 007](spec/007-increment-color-modes.md).

## Standard color map

The conversion API is [light_color.h](../main/light_color.h). `BIG_LIGHT_WHITE` is standard D65 white: `(x=0.3127, y=0.3290)`. It is no longer a manually tuned RGB preset. The initial map uses [standard sRGB primaries and its transfer function](https://registry.color.org/rgb-registry/srgb), with D65 XYZ conversion matrices in [light_color.c](../main/light_color.c). Actual LED primaries and channel strengths are not yet calibrated.

- `light_color_from_rgb(r, g, b)` returns x/y only. `light_brightness_from_rgb(r, g, b)` separately returns linear relative luminance. Use both to preserve the input brightness; sRGB grey `(128,128,128)` has about 0.216 luminance and maps to raw PWM `(55,55,55)`.
- `light_color_from_pwm((light_rgb_t){r,g,b})` converts existing raw LED duty cycles without sRGB gamma decoding. Use `light_brightness_from_pwm(pwm)` alongside it to preserve the original PWM bytes. Startup uses both helpers with the current channel settings in `main.c`.
- `light_color_to_pwm(color, brightness, &pwm)` converts xy/brightness to linear LED duty cycles. If the requested luminance requires any channel above 255, all channels are reduced proportionally to preserve chromaticity. The achieved luminance can therefore be below the request.

Brightness means luminance relative to reference white, not a fraction of a color's maximum channel. To halve a converted color's light output, multiply its existing `brightness` by `0.5f`; do not replace it with `0.5f`. Do not gamma-encode the resulting linear LED PWM again.

Coordinates must be finite and within the sRGB primary triangle: red `(0.64,0.33)`, green `(0.30,0.60)`, blue `(0.15,0.06)`. Out-of-gamut coordinates are rejected rather than extrapolated. Brightness must be finite and within `[0,1]`. These rules also apply when off or at zero brightness. The color helpers map RGB black to D65 xy; the matching brightness helpers return zero. The x/y result alone does not encode black. Invalid conversion requests leave the output argument unchanged.

A future device calibration can replace the output map while retaining standard sRGB input conversion and the public xy/brightness settings.

## Calling the API

Call `lighting_init()` once from `app_main`, before starting other callers. It clears Group A and creates the lighting task. Then submit a complete settings structure:

```c
#include "lighting.h"

big_light_settings_t big_light = {
    .on = true,
    .effect = LIGHT_EFFECT_SOLID,
    .color = BIG_LIGHT_WHITE,
    .brightness = 1.0f,
    .period_ms = 1500,
};

esp_err_t err = set_big_light(&big_light);
// Check err before proceeding.
```

For a color chosen using RGB, or direct xy control:

```c
big_light.color = light_color_from_rgb(255, 255, 0); // Standard yellow.
big_light.brightness = light_brightness_from_rgb(255, 255, 0);
big_light.brightness *= 0.5f;                 // Half its luminance.
esp_err_t err = set_big_light(&big_light);

// Alternatively, reference white at half luminance:
big_light.color = (light_xy_t){.x = 0.3127f, .y = 0.3290f};
big_light.brightness = 0.5f;
err = set_big_light(&big_light);
```

To turn off, retain the same structure, set `big_light.on = false`, and call `set_big_light(&big_light)` again. To restore it, set `on = true` and submit it again. Each call replaces the full settings; it is not a partial update. The caller owns its settings and must not modify them concurrently with a call. The API copies them before returning, so a stack-local structure is sufficient.

For rotation, set `big_light.effect = LIGHT_EFFECT_LIGHT_HOUSE` and `big_light.period_ms` before submitting. Each frame crossfades from the current logical position to its next neighbour, including position 5 back to 0; all other LEDs are black. Positions 0-5 represent one logical 360-degree revolution; the physical direction follows `chain_index` in `group_a.c`.

The minimum lighthouse period remains six RTOS ticks: 60 ms at the configured 100 Hz tick rate. Zero and shorter periods return `ESP_ERR_INVALID_ARG`, including when off. Milliseconds round up to whole ticks. Converted periods must fit within half the tick counter range; the current 32-bit, 100 Hz configuration accepts the full `uint32_t` millisecond range above the minimum. SOLID, CANDLE and SPARKLES ignore `period_ms` and permit zero. At the fixed 24 fps frame rate, short periods can skip positions or appear stationary because frames sample the same rotation phase.

With MONO + STATIC, interpolation splits the converted linear RGB duty cycles between the two LEDs. The incoming channel gets its rounded fraction; the outgoing channel gets the remainder, so their sum exactly matches the selected channel value. No extra gamma curve is applied to fade weights. At an exact position boundary only one LED is lit; between boundaries at most two adjacent LEDs are lit. For example, a 10000 ms period gives 240 frames per revolution.

Every lighthouse settings update, including a period change, restarts the effect at position 0. Off and zero-output brightness clear all LEDs and suspend animation; a solid request immediately displays the selected color pattern on all six LEDs. The owner waits on the settings queue until the next 24 fps frame deadline, so commands can interrupt the wait and render immediately. At the configured 100 Hz tick rate, intervals repeat as 50/40/40/40/40/40 ms, averaging 24 fps. Frame deadlines advance without cumulative transfer-time drift; delayed wakes render the current rotation phase and skip obsolete frames. A failed frame stops automatic updates until another settings request arrives.

The setter does not wait for LED transmission. It enqueues a copy in a four-entry FIFO, with no waiting when full. `ESP_OK` means the request was accepted, not that the LEDs have already changed. The lighting task logs successful application or transfer errors; on failure, the physical LEDs may retain the previous frame. There is no hardware-state getter or completion callback yet.

| Result from `set_big_light` | Meaning |
| --- | --- |
| `ESP_OK` | Complete settings copied into the queue. |
| `ESP_ERR_INVALID_ARG` | Null pointer, unknown effect/color/shift mode, invalid applicable xy endpoint or brightness, invalid lighthouse period, or zero nonstatic shift period. |
| `ESP_ERR_INVALID_STATE` | Lighting has not initialized successfully. |
| `ESP_ERR_TIMEOUT` | Queue full; request rejected immediately. |

Validation errors and a full queue do not enqueue or replace settings. Initialization can also fail due to memory allocation or a driver error. Callers must check these results.

`set_house_lights(const house_lights_settings_t *settings)` defines the Group B API entry point. It currently returns `ESP_ERR_NOT_SUPPORTED` for a non-null argument and `ESP_ERR_INVALID_ARG` for null. It does not configure pins or claim to apply Group B settings.

## Candle effect

Select `LIGHT_EFFECT_CANDLE` with any valid `color`. With MONO + STATIC, its xy hue stays fixed; the algorithm scales each LED's linear channel levels together, so orange, blue, green or other selected colors produce corresponding flames. `brightness` sets the per-LED ceiling, and the flame modulates below it. `period_ms` is ignored and may be zero.

```c
big_light.effect = LIGHT_EFFECT_CANDLE;
big_light.color = light_color_from_rgb(255, 160, 40);
err = set_big_light(&big_light);
```

The six positions receive a low background glow plus a broad, slowly wandering bright region. Small faster changes in its position, shared intensity changes and independent local fluctuations move the illumination pattern without synchronized blinking. Random targets are joined with smooth interpolation at several time scales. There is no fixed revolution or hardcoded orange color.

The effect shares the 24 fps owner-task scheduler. Each settings update starts a new variation; off or zero output sends black and suspends rendering. Switching to solid or lighthouse mode replaces the complete frame immediately. Frames depend on elapsed time and a per-start seed, so delayed updates skip directly to the current flame state. The clock accumulates elapsed ticks across wraparound. Driver failures suspend animation until another request.

The resulting shadow movement depends on LED placement and the model's openings. This is an initial visual model to tune on the actual lighthouse; no physical candle accuracy is claimed.

## Sparkles effect

Select `LIGHT_EFFECT_SPARKLES` (ID 4) with any valid color. Each LED independently brightens quickly and fades more slowly to black. Pseudorandom timing, duration (250-650 ms), and peak intensity vary per LED and pulse; dark gaps separate flashes. Channels scale together below the selected PWM ceiling. `period_ms` is ignored and may be zero.

Sparkles shares the 24 fps scheduler, off/zero-output behavior, mode switching, and transfer-failure handling. Each settings update starts a new variation. Time-based rendering skips missed frames directly and uses the shared accumulated tick clock. Startup keeps the color in `main/main.c`.

Run `python tests/test_light_color.py --suite sparkles` for renderer checks and `--suite lighthouse` for task integration. See [increment 006](spec/006-increment-sparkles.md) for verification status.

## Power behavior

Currently `on = false` sends black to Group A; it does not disconnect LED power. `on = true` with brightness zero also sends black. The MOSFET enable pins remain unused because their circuit belongs to phase 3.

With the future power-switch hardware, the controller will translate `on` into the reserved active-high enable output: GPIO5 for `big_light`, GPIO7 for `house_lights`. The implementation must sequence power, LED startup, and data isolation; BLE code must not write those pins directly.

## BLE integration

Call setters from a normal task-context callback, not an interrupt. The single lighting task serializes updates so radio callbacks do not perform LED transfers. The control layer must coordinate changes from multiple clients because each setter replaces a full settings structure.

The [BLE protocol guide](ble-protocol.md) defines the implemented bondless GATT service, explicit field encodings and error mapping. The BLE host caches the startup and last accepted BLE command; reads report accepted settings, not physical output. Future local controls must share that command state. Do not send raw C structures: float representation, enum sizes and padding are not a wire format. Preserve effect IDs 0, 1, 2 and 4; ID 3 is unused and rejected. Validate incoming field lengths, boolean encodings, and numeric ranges; the setter rejects nonfinite floats and unsupported colors. If accepting RGB bytes, validate before narrowing to `uint8_t`. A successful setter call acknowledges queue acceptance only.

## Verification

Run `python tests/test_light_color.py --suite candle` for the pure flame model: output limits, hue preservation, smooth changes, movement across all six positions, repeatable time-based rendering and different seeds. The lighthouse suite also covers candle start/off/restart, zero output, mode switches, delayed frames, tick wraparound and transfer failure. See [increment 005](spec/005-increment-candle.md).

Run `python tests/test_light_color.py --suite lighthouse` for the actual lighting task with simulated queue, clock and strip calls. This covers 24 fps scheduling, full revolutions, interpolation values and conservation of channel totals, the 5-to-0 transition, off/restart, solid and zero-brightness transitions, delayed wakes, tick wraparound, transfer failure and setter validation. It does not verify the physical LED order or RMT waveform. See [increment 004](spec/004-increment-lighthouse-crossfade.md).

Hardware mapping was confirmed during [increment 001](spec/001-increment-group-a-lighting.md). XY/brightness conversion is tracked in [increment 002](spec/002-increment-xy-brightness.md). Run `python tests/test_light_color.py` on Windows with the installed ESP RISC-V GCC and QEMU tools to verify the actual conversion C code. Hardware color accuracy and off/on behavior after this API change still require a board check.
