# 10: Active Scale Identity

Status: Complete.

## Objective

Make the active scale definition the project truth, retain optional library
provenance, and remove duplicate scale-shift state.

## Prerequisite

Package 07.

## Scope

- Add `ActiveScale` with optional stable `source_id` and embedded definition.
- Add unique stable IDs to library scale YAML.
- Validate scale ID uniqueness and scale/tuning compatibility.
- Preserve source identity through mode shifts.
- Require a current library source ID for scale cycling.
- Fail clearly when the source is absent from the current library.
- Remove `ContentLibraryState::scale_shift_index` and all dependent logic.
- Update serialization, embedded data, commands, and tests.

This is an intentional schema break; do not add name/content inference or legacy
fallbacks.

## Acceptance criteria

- [x] Active scale musical behavior depends on its embedded definition.
- [x] Library provenance is optional and represented only by stable source ID.
- [x] Every shipped library scale has a unique stable ID.
- [x] Mode shifts do not change source ID.
- [x] Cycling fails clearly for embedded/unidentified or missing-library scales.
- [x] Selecting a library scale establishes its source ID.
- [x] `scale_shift_index` and all duplicate active-scale authority are removed.
- [x] Persisted data uses the new schema without a compatibility shim.

## Verification

- [x] Tests cover selection, mode shift, cycling, missing source, duplicate IDs,
      embedded definitions, and serialization round trips.
- [x] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
