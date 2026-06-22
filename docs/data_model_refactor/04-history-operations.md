# 04: Narrow History Operations

## Objective

Replace incidental timeline mutation with explicit commit, guarded amendment, and
root replacement operations.

## Prerequisite

Package 01. This may be implemented in parallel with package 03 after package 02.

## Scope

- Add narrow `commit(project)`, `amend_current(expected_entry_id, project)`, and
  `replace_history(project)` operations.
- Require amendment to target the current entry at the history tip.
- Ensure amendment never truncates redo or modifies an older entry.
- Make replacement install a new root and clear undo/redo.
- Return timeline states by const reference.
- Update callers to copy only when they intend to edit.
- Provide a no-fail installation path for already validated candidates, or document
  and remove remaining failure points.

Do not implement transform session behavior in this package.

## Acceptance criteria

- [x] Commit creates one new entry and truncates redo only under normal commit rules.
- [x] Guarded amendment preserves entry ID, updates project revision, and fails cleanly
      when the expected entry is not current or not at the tip.
- [x] Replacement creates a fresh root identity/revision and leaves no undo/redo.
- [x] Read-only timeline access does not copy project state.
- [x] No handler directly controls history transitions.
- [x] History installation after validation cannot leave partially installed backend
      state.

## Verification

- [x] Tests cover amendment at tip, stale expected ID, amendment with redo present,
      replacement, and const-reference reads.
- [x] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
