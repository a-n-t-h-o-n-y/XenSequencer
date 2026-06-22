# 07: Authoritative Project Validation

Status: Complete.

## Objective

Create one validation boundary for every project entering history or audio
publication.

## Prerequisite

Package 05.

## Scope

- Add `validate(ProjectState const &)` or the equivalent current project type pending
  package 11's rename.
- Validate all invariants listed in the tracker, including recursive model data.
- Run validation after deserialization/import.
- Run validation before history installation and audio snapshot publication.
- Keep request target validation in context resolution.
- Preserve specific user-facing command errors where inputs can be rejected before
  aggregate validation.
- Consolidate or remove duplicated project validation.

## Acceptance criteria

- [x] Invalid project data cannot enter history or reach the audio thread.
- [x] Tuning, base frequency, time signature, duration, scale compatibility, bounds,
      weight, velocity, delay, gate, and recursive invariants are covered.
- [x] Deserialization and every import path use the same authoritative validator.
- [x] Selection/path failures are not reported as project validation failures.
- [x] Duplicate validators no longer disagree or remain authoritative.
- [x] Validation failure leaves history, resources, effects, and publication unchanged.

## Verification

- [x] Focused tests exercise each validation category and at least one nested invalid
      model.
- [x] Tests prove invalid imports and command candidates are atomic failures.
- [x] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
