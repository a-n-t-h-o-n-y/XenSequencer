# 06: Transform and Repeat Sessions

## Objective

Move chord/arpeggio cycling and `again` into backend command-session state, with every
candidate applied to one typed target baseline and at most one amended history entry.

## Prerequisites

Packages 03, 04, and 05.

## Scope

- Add `CommandSessionState`, `TransformCycleSession`, `TransformKind`, and typed
  cell-or-element `TargetSnapshot`.
- Remove transform baselines from editor/project state.
- Restore the original target baseline before applying every chord/arp candidate.
- Implement all continuation checks from the tracker.
- Commit the first changed candidate and guard-amend later compatible candidates.
- Record `again` as parsed command invocations.
- Expand and rebind `again` before transform compatibility evaluation.
- Implement the specified invalidation and preservation rules.
- Clear all command-session state on full project replacement and reset as specified.

## Acceptance criteria

- [ ] Repeated chord/arp candidates never accumulate transforms.
- [ ] A no-op candidate creates no entry, revision, publication, or repeat target but
      preserves a valid transform session.
- [ ] The first changed candidate commits; later compatible candidates amend the same
      entry ID and receive fresh revisions.
- [ ] Kind, target, revision, current entry, or library revision mismatch starts a new
      session.
- [ ] Intervening edits, library reload, undo/redo, reset, load, and replacement
      invalidate transform cycling as specified.
- [ ] Informational and failed commands preserve the session when neither project nor
      library changes.
- [ ] `again` amends only when its expanded chain is exactly one compatible transform.
- [ ] Save/export neither finalizes nor alters transform history.

## Verification

- [ ] Tests cover baseline restoration, no-op-first candidate, amendment identity,
      every invalidation trigger, undo/redo topology, save failure, and `again`.
- [ ] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
