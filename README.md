# Lighthouse

Status: In progress  
Last updated: 2026-09-07

Lighthouse is a battery-powered lighting controller for a laser-cut 3D lighthouse model assembled from flat panels. It will simulate a rotating lighthouse light and illuminate the model's windows using two groups of addressable LEDs.

The model is described here as a **slot-together model**: a construction in which laser-cut planar parts interlock to form a three-dimensional structure. If the parts are instead stacked face-to-face, the corresponding term is **stacked-layer construction**. These assembly methods are described in the [University of Edinburgh's guide to making 3D objects with laser cutting](https://www.ucreatestudio.is.ed.ac.uk/using_laser_cutting_produce_3d_objects).

## Planned hardware

| Component | Purpose |
| --- | --- |
| Seeed Studio XIAO ESP32C3 | Run the lighting effects and the future Bluetooth Low Energy (BLE) interface. |
| Battery | Power the lighthouse. |
| DC-DC converter | Convert the battery voltage to the supply required by the LED groups. |
| Group A: 6 WS2812B LEDs | Simulate the rotating lighthouse light. |
| Group B: 5 WS2812B LEDs | Illuminate the lighthouse windows from inside. |
| MOSFET power switches, planned for phase 3 | Disconnect power to the LED groups during deep sleep. |

Battery specifications, converter selection, and the power-switch circuit will be defined in the implementation specs.

### LED group pin assignments

Each group has one WS2812B data output and one reserved power-enable output for phase 3. Firmware must use the ESP32 GPIO numbers, not the XIAO board labels.

| Group | Signal | XIAO label | ESP32-C3 GPIO | Use |
| --- | --- | --- | --- | --- |
| A | `GROUP_A_DATA` | D2 | GPIO4 | RMT output to the first LED's DIN. |
| A | `GROUP_A_POWER_EN` | D3 | GPIO5 | Reserved control for the future power-switch circuit. |
| B | `GROUP_B_DATA` | D4 | GPIO6 | RMT output to the first LED's DIN. |
| B | `GROUP_B_POWER_EN` | D5 | GPIO7 | Reserved control for the future power-switch circuit. |

These assignments avoid the ESP32-C3 strapping pins GPIO2, GPIO8, and GPIO9. GPIO4-7 also serve external JTAG functions, so that interface cannot use them while they drive the lighting hardware. The assignments leave the board's UART pins and native USB pins available. Board-label mappings and alternate functions are documented in the [Seeed pin map and strapping-pin guidance](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/#pin-map).

Each data output feeds its own chain through a suitable 3.3 V to 5 V logic buffer: six LEDs for Group A and five for Group B. LED power comes from the power supply, not from a GPIO. Both groups share the controller's ground.

The planned power-enable interface is active high: low means off, high means on. The phase 3 circuit must accept 3.3 V logic and provide an external default-off bias during reset and deep sleep. This specifies the interface to a switch circuit, not a direct connection to a 5 V high-side MOSFET gate. Avoiding strapping pins does not guarantee glitch-free outputs during reset; power switching and data isolation must be verified on hardware.

During phase 1, supply the LEDs directly and leave the reserved power-enable connections unused. Independent power switching and protection against powering an unpowered LED through its data input belong to phase 3.

## Lighting groups

### Group A: rotating light effect

Six downward-facing LEDs replace the previous cross layout; there is no center LED. Positions are numbered 0 through 5 and default to the serial chain order. The lighthouse effect crossfades between adjacent LEDs around a logical 360-degree revolution at 24 fps. Set `big_light.period_ms` to the full rotation time in milliseconds; for example, a 10000 ms period gives 240 frames per revolution. The minimum period remains 60 ms, though fast rotations may skip positions at this frame rate. Physical direction follows the chain mapping; change `chain_index` in `main/group_a.c` if needed.

Each frame splits the selected color between two neighbours while preserving their combined channel levels. The [crossfade specification](doc/spec/004-increment-lighthouse-crossfade.md) defines the current animation; the [simple lighthouse specification](doc/spec/003-increment-simple-lighthouse.md) records the earlier on/off implementation.

### Group B: window illumination

Five LEDs will be placed inside the lighthouse to light its windows. This group will provide interior illumination independently of the rotating light effect.

## Development phases

| Phase | Scope | Status |
| --- | --- | --- |
| 1 - LED driver and lighting effects | Implement WS2812B control for both groups, the rotating light animation for group A, and window illumination for group B. | In progress |
| 2 - BLE interface | Add a Bluetooth Low Energy interface for controlling the lighting. Define the supported controls and BLE protocol in the phase's specs. | Planned |
| 3 - Power management | Reduce battery consumption using ESP32C3 deep sleep and MOSFET switches that cut power to both LED groups during sleep. Define sleep entry, wake-up behavior, and power sequencing in the phase's specs. | Planned |

Each phase can be split into smaller, independently verifiable increments using the `doc/spec/NNN-increment-name.md` convention.

## Current state and documentation

The ESP-IDF firmware controls Group A as `big_light` through `set_big_light(const big_light_settings_t *settings)`. Settings include `on`, `effect`, and a `color` containing CIE `x`, `y` and float `brightness` (relative luminance from 0.0 to 1.0). `light_color_from_rgb(r,g,b)` converts standard sRGB input; the default output map uses sRGB primaries and D65 white pending device calibration. A single lighting task converts to raw LED PWM and applies queued updates. Startup selects `LIGHT_EFFECT_CANDLE` using the current raw color in `main.c` through `light_color_from_pwm`. Its six positions combine a wandering bright region with smooth local flicker, all using the selected hue and brightness; candle mode ignores `period_ms`. Solid LEDs retain their frame while powered; animation refreshes at each step. After restoring LED power separately, resubmit settings or reboot the controller for an immediate update.

Connect GPIO4 through the data level buffer to the first LED's DIN, power Group A directly from the LED supply, and connect a common ground. GPIO5-7 remain unused. The [Group A spec](doc/spec/001-increment-group-a-lighting.md) describes the six-position mapping; adjust `chain_index` in `main/group_a.c` if wiring differs from position order. `on = false` currently sends black; MOSFET switching belongs to phase 3.

From an ESP-IDF 6.0 terminal, build with `idf.py build`. To test on the board, use `idf.py -p COM3 flash monitor`, replacing `COM3` if needed. The first build downloads the pinned LED driver. Exit the serial monitor with `Ctrl+]`.

Solid, lighthouse and candle lighting are implemented. Only the color-loop effect remains reserved and returns `ESP_ERR_NOT_SUPPORTED`. Group B is named `house_lights`; its settings type and setter are defined, but the setter also returns `ESP_ERR_NOT_SUPPORTED`. BLE and power management remain planned. See the [lighting API guide](doc/lighting-api.md) for the calling contract and an example.

- [Specification index](doc/README.md): increment status, last updated dates, and workflow conventions.
- [Specification template](doc/SPEC_TEMPLATE.md): starting point for each new increment.
- [Application entry point](main/main.c): firmware implementation.
