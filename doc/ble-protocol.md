# BLE lighting protocol

Status: In progress
Last updated: 2026-09-08
Protocol version: 1

## Connection

The device advertises a custom service as **Lighthouse**. The name is in the scan response, so use active scanning or search by service UUID. Connect without pairing. One connection is supported; advertising stops while connected and resumes after disconnect or a failed connection attempt.

This is bondless, open control: pairing/security-manager support is disabled, no bond keys are stored, and no characteristic requires authentication or encryption. Any nearby BLE client can change the lights. There are no notifications or application-level passwords.

Service UUID: `8e7f0000-8f58-4b5c-9d76-2f5a37c41000`.

## Characteristics

Characteristic UUIDs use `8e7fNNNN-8f58-4b5c-9d76-2f5a37c41000`, replacing NNNN with the hexadecimal number below. Each includes a read-only User Description (0x2901).

Use **Write Request / write with response** for edits, and await the response before the next write. Every control is readable. All payloads fit within the default 23-byte ATT MTU; no MTU negotiation or long writes are needed. No raw C struct bytes are sent.

| NNNN | Control | Bytes | Encoding / permitted values |
| --- | --- | --- | --- |
| 0001 | Protocol/capabilities (read-only) | 5 | `01 17 03 07 01`: version 1; effect bitmask; color-mode bitmask; shift-mode bitmask; supported groups bitmask (A=bit 0, B=bit 1). |
| 0002 | Group A on | 1 | 0=off, 1=on. |
| 0003 | Effect | 1 | 0=SOLID, 1=LIGHT_HOUSE, 2=CANDLE, 4=SPARKLES. ID 3 is invalid. |
| 0004 | Color x,y | 8 | Two little-endian IEEE-754 float32 values, x followed by y. |
| 0005 | Shared brightness | 4 | Little-endian float32, finite and within [0,1]. Relative luminance, as in the lighting API. |
| 0006 | Rotation period | 4 | Little-endian uint32 milliseconds; at least 60 ms for lighthouse at the configured 100 Hz RTOS rate. Ignored by other effects. |
| 0007 | Color mode | 1 | 0=MONO, 1=GRADIENT. |
| 0008 | Gradient end x,y | 8 | Two little-endian float32 values; both endpoints use the shared brightness. |
| 0009 | Shift mode | 1 | 0=STATIC, 1=CYCLE, 2=RANDOM. |
| 000a | Shift period | 4 | Little-endian uint32 milliseconds; nonzero for CYCLE/RANDOM. Ignored by STATIC. |
| 000b | Group B on | 1 | Reads 0. Writes return application error 0x80 until Group B is implemented. Capability bit B is clear. |

Coordinates must be finite and inside the supported sRGB triangle, including gradient endpoint writes while MONO is selected. The [lighting API guide](lighting-api.md) defines color interpolation, effects and brightness behavior.

## Write semantics and errors

Each accepted field write patches the cached command and submits the complete settings to `set_big_light()`. The NimBLE host serializes these writes, while the lighting task performs LED transfers. Invalid writes and a full lighting queue leave cached settings unchanged.

A successful write means **queued**, not physically applied. Reads return the most recent accepted command, including updates that may still be waiting in the queue. A subsequent LED transfer failure is logged on serial; GATT readback is not measured hardware state.

The initial cache is seeded from the startup command in `main/main.c`. There are no later local control writers in this firmware. Future buttons or other local controls must share this command state rather than bypassing BLE with direct setter calls, otherwise readback and subsequent field patches can be stale.

Writes to different characteristics are not an atomic transaction. Set a valid gradient endpoint before selecting GRADIENT, a nonzero shift period before selecting CYCLE/RANDOM, and a valid rotation period before selecting LIGHT_HOUSE. Every accepted update restarts animation timing according to the lighting API. For setup without intermediate visible changes, turn Group A off, update settings, then turn it on.

| ATT error | Meaning |
| --- | --- |
| 0x03 | Write not permitted (read-only protocol/capabilities). |
| 0x0d | Wrong payload length; exact lengths are required. |
| 0x13 | Invalid boolean, enum, coordinate, brightness or applicable period. |
| 0x11 | Lighting queue full or insufficient resources; retry after a short delay. |
| 0x0e | Internal error or lighting not initialized. |
| 0x80 | Group B is not supported by this firmware. |

Disconnecting leaves the last accepted settings active. Reconnecting reads those values. Rebooting restores the startup defaults in `main/main.c`; BLE edits are not persisted.

## Example payloads

- Group A off: write `00` to characteristic 0002; on: `01`.
- Sparkles: write `04` to 0003.
- Brightness 0.5: write `00 00 00 3f` to 0005.
- Rotation period 15,000 ms: write `98 3a 00 00` to 0006.
- Gradient mode: write `01` to 0007, after setting its endpoint.
- Random shifting: write `02` to 0009, after setting a nonzero shift period.
- Shift period 7,000 ms: write `58 1b 00 00` to 000a.

Python's standard library can produce the exact bytes for a client or a BLE testing application's hex editor:

```python
import struct

print(struct.pack('<ff', 0.15, 0.06).hex(' '))  # Blue endpoint for 0008
print(struct.pack('<f', 0.5).hex(' '))         # Brightness for 0005
print(struct.pack('<I', 10000).hex(' '))       # 10-second period for 000a
```

## Build and verification

Console output uses the board's native USB Serial/JTAG port as the primary console. Open the USB serial monitor before connecting. Expected INFO messages include `BLE identity`, `Advertising as Lighthouse`, `BLE connected: handle=...`, `GATT read/write`, and `BLE disconnected: handle=... reason=...`. These are firmware serial logs, not BLE notifications.

If services can be read but connection logs are missing, confirm the new firmware was flashed, remove serial-monitor tag filters, and check that `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`. The previous UART0-primary / USB-secondary configuration used non-blocking secondary output, which can lose messages. The logging change addresses that possible cause; it does not establish why a particular earlier message was missing.

The tracked `sdkconfig.defaults` enables NimBLE, peripheral/server roles and a single connection, and disables pairing. The existing local sdkconfig has also been updated. An older sdkconfig takes precedence over defaults: in `idf.py menuconfig`, enable Bluetooth/NimBLE and disable NimBLE Security (SMP), with maximum connections set to one. Alternatively, build with a new SDKCONFIG path and these defaults. The source deliberately rejects a build with pairing enabled.

Build with `idf.py build`. NVS initializes for radio calibration; no automatic erase is performed if NVS initialization fails. Errors are logged and local lighting continues. Advertising starts asynchronously after host synchronization; check the serial `Advertising as Lighthouse` message.

Run `python tests/test_light_color.py --suite ble` for encoding and command handling against the real lighting setter. Existing color and lighting suites remain applicable. These tests do not simulate the BLE radio. The [BLE increment](spec/008-increment-ble-lighting.md) tracks connect/read/write/reconnect and RMT coexistence checks on the board.

Initialization follows the [ESP-IDF 6.0 NimBLE sequence](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c3/api-reference/bluetooth/nimble/index.html), checked against the installed ESP-IDF peripheral example and headers. The host task is created explicitly to check allocation failure.
