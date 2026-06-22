# 01: History Identity and Project Revisions

## Objective

Introduce distinct typed identities for undo/redo entries and authoritative project
generations. Stop using prospective commit IDs or mixed snapshot versions as stale
edit guards.

## Scope

- Add `HistoryEntryId` as the immutable identity of a history entry.
- Add monotonic, process-local `ProjectRevision`.
- Expose the current entry ID and revision with the current project.
- Advance revision on effective commit, amendment, undo, redo, and project-context
  replacement.
- Allocate a fresh revision for host restoration, including restoration of equal data.
- Keep revisions out of persisted project files.
- Make equal-project and failed operations preserve the current revision.
- Update focused history and processor tests.

Do not redesign command contexts or history mutation APIs in this package.

## Acceptance criteria

- [ ] Entry identity and project revision are different strong types.
- [ ] An amended entry can retain its entry ID while receiving a new revision.
- [ ] Undo/redo changes revision even when returning to a previously visited entry.
- [ ] Host restoration invalidates requests from the prior project context.
- [ ] No persisted project schema contains process-local IDs or revisions.
- [ ] Existing code no longer treats `get_next_commit_id()` or `snapshot_version` as
      the authoritative edit guard.
- [ ] Failed and no-op project operations leave identity and revision unchanged.

## Verification

- [ ] Focused tests cover commit, amendment-ready identity semantics, undo, redo,
      equal-data restoration, and no-op behavior.
- [ ] `XenTests` builds.
- [ ] Full `ctest` passes, or the blocker is recorded.
