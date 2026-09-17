# 013 - PMIC keep-alive

Status: In progress  
Last updated: 2026-09-17

## Purpose

Keep the PMIC supplied with the requested MCU-alive signal on XIAO D1 / GPIO3.

## Requirements

- R1: Release GPIO3 to high impedance at application initialization, with internal pull-up and pull-down disabled.
- R2: Pull low for 20 ms every 25 seconds, measured between pulse starts, then release to high impedance. Start the first pulse after 25 seconds.
- R3: Run independently of lighting and BLE without blocking their tasks. Never actively drive the signal high.

## Implementation approach

`main.c` initializes an open-drain output before lighting startup. A periodic ESP-IDF timer starts each low pulse; a one-shot timer releases the output after 20,000 microseconds. The periodic timer skips missed events instead of generating catch-up pulses. Timer callbacks run in the ESP timer task and are subject to scheduling latency.

## Acceptance criteria

- [x] AC1: ESP32-C3 firmware builds successfully.
- [ ] AC2: Hardware measurement confirms initial HZ, a first pulse after 25 seconds, and 20 ms low pulses every 25 seconds while lighting and BLE are active.
- [ ] AC3: Hardware measurement confirms the output is released between pulses with no internal pull resistors enabled.

## Verification

ESP-IDF 6.0 `idf.py build` passed on 2026-09-17, including application partition size checks (56% free). Source review confirmed pull resistors are disabled and the output latch is preloaded to release before open-drain output is enabled. `git diff --check` passed. Hardware waveform and PMIC operation verification remain pending; the board has not been flashed as part of this increment.

## Dependencies and open questions

The user specified the waveform. The PMIC electrical interface and reset-time behavior have not been verified on hardware. Deep-sleep integration is outside this increment; timers operate while the MCU is awake.
