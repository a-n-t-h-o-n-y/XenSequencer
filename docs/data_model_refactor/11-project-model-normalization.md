# 11: Project Model Normalization

## Objective

Replace the mixed engine state with a persisted musical `ProjectState` containing a
measure and grouped pitch system only.

## Prerequisites

Packages 07 and 10.

## Scope

- Rename `EngineState` to `ProjectState` or `SequencerProject`.
- Add `NamedTuning` and `PitchSystem` with the fields defined in the tracker.
- Move tuning name into embedded display metadata.
- Remove selection, history, library, command session, workspace, UI, and transport
  data from the project aggregate.
- Update command, rendering, serialization, history, bridge, and test consumers.
- Apply the intentional persisted project schema break.
- Keep `PluginState`, if still present, only as a composition root.

## Acceptance criteria

- [ ] The project aggregate contains only persisted musical state needed for rendering.
- [ ] Coupled tuning, scale, transposition, translation, and base-frequency fields live
      under one pitch-system object.
- [ ] Tuning name is not treated as a library lookup key.
- [ ] Project serialization reflects the new structure with no compatibility path.
- [ ] History stores only project snapshots and associated history metadata.
- [ ] Audio publication consumes validated immutable project render data.
- [ ] No duplicated old `EngineState` structure or field authority remains.

## Verification

- [ ] Serialization and host-state tests use the new schema.
- [ ] Command and rendering tests cover the grouped pitch fields.
- [ ] `XenCore`, `XenTests`, and full `ctest` pass, or blockers are recorded.
