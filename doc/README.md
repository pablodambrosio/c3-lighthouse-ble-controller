# Specification index

Status: Active  
Last updated: 2026-09-07

This folder contains plain Markdown development specifications. Each spec describes one increment: a small, independently verifiable change.

See the [project README](../README.md) for the lighthouse concept, planned hardware, and development phases.

The [lighting API guide](lighting-api.md) documents the `big_light` and `house_lights` settings and control functions.

## Specifications

| ID | Increment | Status | Last updated |
| --- | --- | --- | --- |
| 000 | [Documentation foundation](spec/000-increment-documentation.md) | Completed | 2026-09-05 |
| 001 | [Group A lighting](spec/001-increment-group-a-lighting.md) | In progress | 2026-09-07 |
| 002 | [XY and brightness](spec/002-increment-xy-brightness.md) | Completed | 2026-09-07 |
| 003 | [Simple lighthouse rotation](spec/003-increment-simple-lighthouse.md) | Completed | 2026-09-07 |
| 004 | [Lighthouse crossfade](spec/004-increment-lighthouse-crossfade.md) | Completed | 2026-09-07 |
| 005 | [Candle with moving illumination](spec/005-increment-candle.md) | Completed | 2026-09-07 |

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
