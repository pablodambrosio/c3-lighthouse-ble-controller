# 000 - Documentation foundation

Status: Completed  
Last updated: 2026-09-05

## Purpose

Establish a plain Markdown specification workflow so development can be described and tracked in small increments.

## Scope

- Included: a documentation folder, numbered increment specs, a reusable template, and a spec index.
- Excluded: firmware changes and definitions of future product features.

## Requirements

- R1: Store increment specs under `doc/spec/` using the `NNN-increment-name.md` naming convention.
- R2: Provide an index linking to every increment spec and showing its status and last updated date.
- R3: Include status and last updated date in every spec.
- R4: Provide a reusable template with requirements, acceptance criteria, and verification sections.

## Implementation approach

Use `doc/README.md` as the index and workflow guide. Use `doc/SPEC_TEMPLATE.md` to create new specs. Track this documentation setup as increment 000.

## Acceptance criteria

- [x] AC1: This spec exists at `doc/spec/000-increment-documentation.md`.
- [x] AC2: The index links to this spec and lists its matching status and date.
- [x] AC3: This spec and the template contain status and last updated fields.
- [x] AC4: The template contains requirements, acceptance criteria, and verification sections.

## Verification

On 2026-09-05, checked the documentation file paths, local Markdown links, metadata, index entry, and required template sections. All acceptance criteria passed.

## Dependencies and open questions

None.
