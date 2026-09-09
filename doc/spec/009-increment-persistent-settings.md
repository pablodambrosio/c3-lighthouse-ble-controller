# 009 - Persistent lighting settings

Status: In progress  
Last updated: 2026-09-09

## Purpose

Restore the user's Group A configuration after reboot. When no usable saved configuration exists, use the current defaults defined in `main/main.c`.

## Scope

- Included: NVS storage, startup restoration and validation, automatic saving of accepted BLE changes, grouped writes, error logging and documentation.
- Excluded: Group B persistence, animation-phase restoration, factory-reset controls, record migration, new GATT characteristics and hardware flashing.

## Requirements

- R1: Save every Group A setting: on/off, effect, start x/y, shared brightness, effect period, color mode, gradient-end x/y, shift mode and shift period.
- R2: Load saved settings before submitting the startup lighting command and initializing BLE readback. Both must start with the same configuration. Restore settings, not animation progress.
- R3: Missing records, read errors, unexpected record sizes, unsupported versions or invalid settings must leave the defaults from `main.c` intact. Validate saved settings using the same rules as the lighting setter, including its treatment of inactive fields.
- R4: Schedule a save only after an accepted BLE write. Use the latest complete settings and wait for two seconds without another scheduled update before writing. Skip records identical to the last successfully saved record. Rejected writes must not schedule storage changes.
- R5: Keep flash writes outside the BLE callback and lighting task. Log successful saves and storage failures. Storage initialization or save failures must not stop local lighting; never automatically erase unrelated NVS data.
- R6: GATT success and readback confirm accepted lighting commands, not flash persistence. Removing power before a save completes may lose recent edits. Individual BLE writes remain separate commands; the quiet interval groups changes but does not make a client operation atomic.

## Implementation approach

`main/light_storage.c` initializes NVS, opens namespace `lighting`, and reads the `group_a` blob. Version 1 contains twelve fixed-width 32-bit words (48 bytes): version, on, effect, color x, color y, brightness, period, color mode, gradient-end x, gradient-end y, shift mode and shift period. Floating-point fields store IEEE-754 bit patterns; words use ESP32 little-endian order. The record does not depend on the layout of `big_light_settings_t`.

`lighting_validate()` validates without enqueueing a command and is shared by restoration and `set_big_light()`. Startup retains the existing default initializer, attempts restoration, then submits the resulting settings and seeds BLE with them. Defaults are not automatically saved just by booting.

After a successful GATT write, `light_storage_schedule()` overwrites a one-entry queue with the latest complete settings. A dedicated worker waits for two quiet seconds, calls `nvs_set_blob()` and `nvs_commit()`, and logs `Group A settings saved` on success. A failed save is logged; it is not retried automatically without a later accepted write. Later local control sources must explicitly join the save path and keep BLE readback coherent.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds and fits the application partition; existing lighting and BLE regression suites pass after validation is extracted.
- [ ] AC2: With no saved record, boot uses the unchanged `main.c` defaults and BLE reads the same values. Invalid length/version/settings also fall back without partially replacing defaults.
- [ ] AC3: Change all supported fields over BLE, wait for the save log, reboot and verify complete restoration, including off state, floating-point colors/brightness and effect periods.
- [ ] AC4: Verify a burst of accepted writes produces the final saved configuration after the quiet interval; rejected writes and identical configurations do not produce unnecessary commits.
- [ ] AC5: Verify storage errors and interrupted saves leave lighting usable and do not erase unrelated NVS. Verify behavior under BLE traffic and running LED effects.

## Verification

2026-09-09: ESP-IDF 6.0 build passed, producing a `0x6dc70`-byte firmware image with 57% of the application partition free. The `lighthouse` and `ble` RISC-V/QEMU regression suites passed using production validation and command handling. These suites do not exercise NVS or the save worker.

Source review confirms the versioned record, full-settings validation, startup ordering, accepted-write hook, one-entry queue, quiet interval, unchanged-record comparison and explicit commit. Storage-specific fault/record tests and physical reboot verification have not been performed. No board was flashed for this increment; AC2-AC5 remain pending.

## Dependencies and open questions

Extends [increment 008](008-increment-ble-lighting.md) and its [BLE protocol](../ble-protocol.md). This increment supersedes 008's original reboot-to-default behavior and adds lighting storage alongside existing NVS use. The [HTML controller](../../tools/html/README.md) requires no new protocol fields.

Changing defaults in a later firmware build does not override valid saved settings. A user-facing reset-to-default action and migration policy are future work. Power management must account for pending saves before entering deep sleep.
