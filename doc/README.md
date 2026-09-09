# Specification index

Status: Active  
Last updated: 2026-09-10

This folder contains plain Markdown development specifications. Each spec describes one increment: a small, independently verifiable change.

See the [project README](../README.md) for the lighthouse concept, planned hardware, and development phases.

The [lighting API guide](lighting-api.md) documents the `big_light` and `house_lights` settings and control functions.

## Phase 1 progress

Group A's light-control feature set is complete as of 2026-09-08: solid, lighthouse, candle and sparkles effects; MONO/GRADIENT colors; STATIC/CYCLE/RANDOM shifting; and separate shared brightness. Increments 002-007 are completed.

Increment 001 remains In progress solely for six-LED hardware verification: mapping/color order, visual behavior and a 10-minute stability run. Group B window lighting is implemented in increment 011; hardware verification remains pending. Phase 1 therefore remains In progress. Phase 2 BLE control is underway in increment 008; phase 3 power management remains planned.

The [BLE protocol guide](ble-protocol.md) defines bondless connection behavior, GATT UUIDs, wire formats and error handling.

Increment 009 adds persistent Group A settings with automatic saving and reboot restoration. Implementation and build checks are complete; storage-specific and hardware verification remain pending.

## Specifications

| ID | Increment | Status | Last updated |
| --- | --- | --- | --- |
| 000 | [Documentation foundation](spec/000-increment-documentation.md) | Completed | 2026-09-05 |
| 001 | [Group A lighting](spec/001-increment-group-a-lighting.md) | In progress | 2026-09-08 |
| 002 | [XY and brightness](spec/002-increment-xy-brightness.md) | Completed | 2026-09-07 |
| 003 | [Simple lighthouse rotation](spec/003-increment-simple-lighthouse.md) | Completed | 2026-09-07 |
| 004 | [Lighthouse crossfade](spec/004-increment-lighthouse-crossfade.md) | Completed | 2026-09-07 |
| 005 | [Candle with moving illumination](spec/005-increment-candle.md) | Completed | 2026-09-07 |
| 006 | [Sparkles](spec/006-increment-sparkles.md) | Completed | 2026-09-08 |
| 007 | [Color modes and shifting](spec/007-increment-color-modes.md) | Completed | 2026-09-08 |
| 008 | [Bondless BLE lighting control](spec/008-increment-ble-lighting.md) | In progress | 2026-09-08 |
| 009 | [Persistent lighting settings](spec/009-increment-persistent-settings.md) | In progress | 2026-09-09 |
| 010 | [BLE connection indicators](spec/010-increment-ble-indicators.md) | In progress | 2026-09-09 |
| 011 | [Four-LED Group B lighting](spec/011-increment-group-b-lighting.md) | In progress | 2026-09-09 |
| 012 | [Device settings service](spec/012-increment-device-settings.md) | In progress | 2026-09-10 |

## Conventions

- Store specs as `doc/spec/NNN-increment-name.md`, for example `001-increment-display-startup.md`.
- Use a unique, three-digit sequential ID and a short lowercase name separated by hyphens. Keep IDs stable; do not reuse them.
- Copy [SPEC_TEMPLATE.md](SPEC_TEMPLATE.md) for each new increment and replace all placeholders.
- Include a status and last updated date in every spec. Use dates in `YYYY-MM-DD` format.
- Add each spec to the table above. Whenever a spec changes, update its date and keep its index row's status and date in sync. Also update this index's date when editing it.
- Keep the problem, requirements, and acceptance criteria clear before implementation. Record verification results before marking an increment completed.

## Spec statuses

| Status | Meaning |
| --- | --- |
| Draft | The increment is being defined. |
| Ready | Scope and acceptance criteria are clear enough to implement. |
| In progress | Implementation or verification is underway. |
| Blocked | Progress depends on an unresolved issue recorded in the spec. |
| Completed | Acceptance criteria have been met and verification is recorded. |
| Cancelled | The increment will not be implemented; the reason is recorded in the spec. |
