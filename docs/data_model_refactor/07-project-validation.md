# 07: Authoritative Project Validation

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

- [ ] Invalid project data cannot enter history or reach the audio thread.
- [ ] Tuning, base frequency, time signature, duration, scale compatibility, bounds,
      weight, velocity, delay, gate, and recursive invariants are covered.
- [ ] Deserialization and every import path use the same authoritative validator.
- [ ] Selection/path failures are not reported as project validation failures.
- [ ] Duplicate validators no longer disagree or remain authoritative.
- [ ] Validation failure leaves history, resources, effects, and publication unchanged.

## Verification

- [ ] Focused tests exercise each validation category and at least one nested invalid
      model.
- [ ] Tests prove invalid imports and command candidates are atomic failures.
- [ ] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
