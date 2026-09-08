# 008 - Bondless BLE lighting control

Status: In progress
Last updated: 2026-09-08

## Purpose

Initialize BLE and expose all current lighting controls through a simple GATT service. Interpret the requested bondless operation as no pairing, encryption requirement or stored bonds.

## Scope

- Included: NimBLE peripheral, one connection, advertising/reconnect, readable/writable controls, explicit wire format, command validation, protocol tests and firmware build.
- Excluded: phone app, notifications, settings persistence, Group B driver, power management and board flashing.

## Requirements

- R1: Initialize NVS and NimBLE, then advertise as Lighthouse with the custom lighting service. Resume advertising after disconnect or failed connection. Initialization errors must leave local lighting running and be logged.
- R2: Disable pairing/security-manager and bond persistence. No GATT authentication flags or allowlist. Any nearby client can control the device; only one client connects at a time.
- R3: Expose protocol/capabilities plus Group A on, effect, x/y color, brightness, rotation period, color mode, gradient endpoint, shift mode and shift period. Expose Group B on/off with explicit unsupported-write behavior while the driver is absent.
- R4: Encode booleans/enums as bytes, periods as little-endian uint32 milliseconds, and x/y/brightness as little-endian IEEE-754 float32. Never transmit raw C structs. All values fit the default ATT MTU.
- R5: Each write patches one field of the last accepted command and submits the full settings through the existing setter. Reject invalid lengths, values and queue-full requests without changing the readback cache. Readback acknowledges accepted commands, not physical output. Seed the cache from the startup command; subsequent application control must use the BLE control layer to keep it coherent.
- R6: Preserve startup settings and current lighting after disconnect. A reboot restores main.c defaults. Individual characteristic writes are independently applied; cross-field changes require valid intermediate settings.

## Implementation approach

Use the installed ESP-IDF 6.0 NimBLE port and a small protocol module independent of the radio stack. The NimBLE host serializes GATT access; the existing lighting task owns LED transfers. The protocol guide defines UUIDs, sizes, error codes and client examples.

## Acceptance criteria

- [x] AC1: BLE-enabled ESP32-C3 firmware builds and fits the application partition.
- [x] AC2: Protocol tests cover every field, endian encoding, exact lengths, invalid values, rejected-write preservation and unsupported Group B control. Lighting regressions pass.
- [ ] AC3: On hardware, discover the advertised service, connect without pairing, read/write each supported control, check errors, disconnect and reconnect.
- [ ] AC4: Verify animations under BLE traffic without unexpected flashes, resets or RMT transfer errors.

## Verification

On 2026-09-08, the ESP-IDF 6.0 BLE-enabled build passed and produced a 446,496-byte firmware image, leaving 57% of the application partition free. Generated sdkconfig confirms NimBLE enabled, security-manager disabled and maximum connections set to one. The tracked defaults reproduce those settings.

All six RISC-V/QEMU suites passed: ble, pattern, lighthouse, candle, sparkles and color. BLE tests exercise the real lighting setter with simulated queue/task/driver calls, covering all fields, little-endian reference bytes, exact lengths, invalid booleans/enums/coordinates/brightness/periods, uint32 limits, queue-full preservation and unsupported Group B writes. Existing tests retain their test-linker RWX segment warning.

The host-task creation result is explicitly checked because the installed esp_nimble_enable wrapper ignores allocation failure. The BLE stack and GATT registration compile against installed IDF headers; protocol tests do not run the radio or host connection lifecycle.

No board was flashed. Radio behavior, advertising/reconnect and BLE/RMT coexistence remain unchecked; this increment stays In progress until AC3 and AC4 are recorded.

## Dependencies and open questions

Hardware feedback (2026-09-08): the user sees advertising on the USB serial monitor and confirms BLE Scanner displays the custom service and characteristics. The user subsequently clarified that reading produces no visible value, so successful characteristic reads are not yet established. Connect/disconnect serial messages were not visible. Diagnostics now include identity, explicit connection/disconnection handles, MTU and GATT access results, and native USB is the primary console instead of secondary output. The cause of the missing messages is not confirmed; verify the revised firmware on the board. Service visibility alone does not establish successful read/write/reconnect behavior. AC3 and AC4 remain pending.

Uses completed Group A controls (increments 002-007). Group B remains a separate pending increment. NVS is used for radio calibration, not bonds or lighting settings; initialization does not automatically erase existing NVS contents.
