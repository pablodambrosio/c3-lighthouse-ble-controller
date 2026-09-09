# 012 - Device settings service

Status: In progress  
Last updated: 2026-09-10

## Purpose

Provide persistent device-wide boot and BLE indicator preferences in a separate settings service.

## Scope

- Included: boot policy for both groups, connection-indicator toggle, versioned NVS record, separate readable/writable GATT service and HTML controls.
- Excluded: remote reboot, factory reset, changing lighting defaults over BLE, authentication and hardware flashing.

## Requirements

- R1: Expose `boot_behavior`: 0=LAST_STATE, 1=DEFAULT, 2=OFF. LAST_STATE restores saved lighting settings with firmware defaults as fallback. DEFAULT uses the initializers in `main.c` for both groups. OFF restores saved settings (or defaults) but forces both groups off before their first submitted frame.
- R2: Expose `ble_connection_indicator_enabled`: 0=false, 1=true. Gate future successful-connect blue pulses and disconnect red flashes on Group A. Do not cancel an indicator already queued or running.
- R3: With no valid device-settings record, default to LAST_STATE and enabled indicators, preserving previous behavior.
- R4: Commit valid changes to NVS before reporting GATT write success. Reject invalid lengths/values. Failed saves leave the runtime/readback settings unchanged and return an error. Skip unchanged writes.
- R5: Changing boot policy does not immediately alter output, erase saved lighting settings or disable ordinary lighting persistence. The policy applies next boot. OFF does not prevent a later indicator when indicators are enabled.
- R6: Keep the existing lighting service and UUIDs unchanged. Offer device settings in the HTML controller with explicit save and readback.

## Implementation approach

`device_settings.c` stores a three-byte record in namespace `device_settings`, key `config`: version 1, boot behavior, indicator enabled. Initialization precedes lighting-state restoration. `main.c` preserves each group's default initializer, loads lighting storage, applies policy, then submits the resulting settings and seeds BLE readback.

The NimBLE host owns runtime preference writes. These infrequent writes synchronously call `nvs_set_blob()` and `nvs_commit()`; unlike lighting edits they do not use the two-second debounce. The settings callback validates one-byte requests. Preferences are independent of the per-group lighting records. No NVS erase is performed on errors.

Service UUID: `8e7f0100-8f58-4b5c-9d76-2f5a37c41000`.

| Characteristic UUID prefix | Name | Encoding |
| --- | --- | --- |
| `8e7f0101` | `boot_behavior` | One byte: 0, 1 or 2 |
| `8e7f0102` | `ble_connection_indicator_enabled` | One byte: 0 or 1 |

Both UUIDs share the service's remaining suffix. Both are readable and writable with response and have User Description descriptors. Wrong lengths return ATT 0x0d, invalid values 0x13, and storage/uninitialized errors 0x0e. Service access remains bondless and unencrypted. Advertising continues to identify the original lighting service because the legacy advertising packet cannot contain both 128-bit UUIDs; discover the settings service after connection. Web Bluetooth clients must request it in `optionalServices`.

## Acceptance criteria

- [x] AC1: Tests cover default behavior, all three boot policies, invalid values, indicator state, unchanged-write suppression, commit failure and restoration of committed preferences.
- [x] AC2: ESP32-C3 build and partition-size checks pass.
- [ ] AC3: On hardware, discover/read/write the settings service and reboot under each policy with distinct saved/default configurations for both groups.
- [ ] AC4: Verify enabled/disabled indicators, persistence across reboot, HTML controls and storage-error behavior on hardware.

## Verification

2026-09-10: RISC-V/QEMU BLE regression suite passes with production device-settings code and simulated NVS. JavaScript syntax check passes. ESP-IDF 6.0 build passes with a `0x6f510`-byte firmware image and 57% of the application partition free. These tests do not verify real flash commits, radio traffic or physical output. Hardware tests remain pending; no board flashed.

## Dependencies and open questions

Extends [persistent settings](009-increment-persistent-settings.md), [BLE indicators](010-increment-ble-indicators.md) and [Group B](011-increment-group-b-lighting.md). A disconnect immediately after disabling indicators should not flash red. OFF with indicators enabled can still produce a temporary connection pulse; disable indicators as well when no connection signaling is wanted.
