# 03: Selection at the Command Boundary

## Objective

Make selection frontend-owned request context, reject stale positional targets, and
return deterministic suggestions after structural edits.

## Prerequisite

Package 02.

## Scope

- Rename `SelectedState` to `SelectionPath` or `SelectionAddress`.
- Remove active selection from backend persisted/domain state.
- Accept selection beside command text in the bridge request context.
- Resolve targets only after validating the expected project revision.
- Reject missing required selections and paths that do not resolve.
- Return optional selection suggestions from command application.
- Allow a suggestion to become the next target inside one command chain.
- Move selection navigation and input-mode commands to frontend actions where they
  still exist in backend command handling.
- Update bridge serialization and focused command tests.

Frontend sibling changes are included only where required to consume the revised
bridge contract.

## Acceptance criteria

- [ ] The authoritative backend project contains no active selection or input mode.
- [ ] Targeted commands require both a current revision and a resolvable selection.
- [ ] Positional paths are rejected when stale; no rebasing or stable-ID workaround is
      introduced.
- [ ] Structural edits return the deterministic suggestions listed in the tracker.
- [ ] Non-structural transforms return the supplied selection when successful.
- [ ] Command chains can use an earlier command's suggestion without mutating global
      selection state.
- [ ] Backend navigation/input-mode command paths made obsolete by frontend ownership
      are removed.

## Verification

- [ ] Tests cover stale revision, missing selection, invalid path, and each structural
      suggestion category.
- [ ] Bridge contract tests cover request context and response suggestion encoding.
- [ ] Affected backend/frontend tests and full backend `ctest` pass, or blockers are
      recorded.
