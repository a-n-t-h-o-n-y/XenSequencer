# 13: Final Cleanup and Invariant Audit

Status: Backend audit complete; frontend pending.

## Objective

Remove migration leftovers and prove the final implementation satisfies the tracker’s
non-negotiable invariants as one coherent architecture.

## Prerequisites

Packages 06 through 12.

## Scope

- Search for and remove obsolete state fields, APIs, endpoints, effect enums, command
  contexts, completion paths, validators, and serialization fields.
- Remove temporary migration diagnostics and adapters.
- Audit command policies against actual handler capabilities and effects.
- Audit every project mutation/publication path for revision and validation behavior.
- Audit command-session invalidation and lifetime behavior.
- Audit frontend/backend ownership boundaries.
- Update architecture and bridge documentation to match the implemented system.
- Run the complete verification workflow.

## Acceptance criteria

- [x] No project-aware command runs without an expected current revision.
- [x] Stale positional selections are always rejected and never rebased.
- [x] Handlers receive only declared capabilities.
- [x] A command chain installs at most one project-history transition.
- [x] Chord/arp cycling uses a backend baseline and amends at most one committed entry.
- [x] Project, library, command-session, UI, workspace, and transport state have
      separate ownership.
- [x] No whole-`PluginState` command transaction copy remains.
- [x] No removed endpoint, old schema field, compatibility shim, or duplicate source of
      truth remains.
- [ ] Documentation describes only the final architecture.

## Verification

- [x] Configure with `./configure.sh` if required by source/CMake changes.
- [x] Build the relevant aggregate targets with `cmake --build build`.
- [x] Run `ctest --test-dir build --output-on-failure` successfully.
- [x] Run `git diff --check`.
- [x] Review the complete diff for scope growth, stale comments, temporary code, and
      unintended generated artifacts.
