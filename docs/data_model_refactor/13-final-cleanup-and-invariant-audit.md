# 13: Final Cleanup and Invariant Audit

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

- [ ] No project-aware command runs without an expected current revision.
- [ ] Stale positional selections are always rejected and never rebased.
- [ ] Handlers receive only declared capabilities.
- [ ] A command chain installs at most one project-history transition.
- [ ] Chord/arp cycling uses a backend baseline and amends at most one committed entry.
- [ ] Project, library, command-session, UI, workspace, and transport state have
      separate ownership.
- [ ] No whole-`PluginState` command transaction copy remains.
- [ ] No removed endpoint, old schema field, compatibility shim, or duplicate source of
      truth remains.
- [ ] Documentation describes only the final architecture.

## Verification

- [ ] Configure with `./configure.sh` if required by source/CMake changes.
- [ ] Build the relevant aggregate targets with `cmake --build build`.
- [ ] Run `ctest --test-dir build --output-on-failure` successfully.
- [ ] Run `git diff --check`.
- [ ] Review the complete diff for scope growth, stale comments, temporary code, and
      unintended generated artifacts.
