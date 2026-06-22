# Data Model Refactor Work Packages

This directory turns
[`data_model_refactor_tracker.md`](../data_model_refactor_tracker.md) into focused
implementation assignments. The tracker remains the architecture source of truth.
These files define sequencing, scope, and completion criteria.

## How to use these files

- Give one package file to an implementation agent at a time.
- Complete packages in numeric order unless the package explicitly says it can run in
  parallel.
- Treat prerequisites as hard dependencies. Do not add temporary compatibility paths
  to bypass them.
- Keep each package limited to its stated scope. If implementation reveals a required
  architectural change, update the tracker and affected package before proceeding.
- A package is complete only when all acceptance and verification checkboxes are
  satisfied, or an unchecked item is documented as blocked with a concrete reason.
- Check off completed criteria in the package file as part of the implementation.

## Package sequence

| Package | Outcome | Prerequisites | Status |
| --- | --- | --- | --- |
| [01](01-history-identity-and-revisions.md) | Separate history-entry identity from project revision | None | Complete |
| [02](02-command-context-and-policy.md) | Add typed request context and explicit command policies | 01 | Pending |
| [03](03-selection-command-boundary.md) | Move selection ownership to the frontend boundary | 02 | Pending |
| [04](04-history-operations.md) | Provide commit, guarded amendment, and replacement APIs | 01 | Pending |
| [05](05-command-transaction-and-capabilities.md) | Replace whole-state copies with scoped transactions | 02, 04 | Pending |
| [06](06-transform-and-repeat-sessions.md) | Implement baseline-based chord/arp cycling and `again` | 03, 04, 05 | Pending |
| [07](07-project-validation.md) | Establish one authoritative project validation boundary | 05 | Pending |
| [08](08-command-catalog-delivery.md) | Deliver immutable catalog metadata at session startup | 02 | Pending |
| [09](09-resource-publication.md) | Split project, library, session, and transport publication | 03, 05, 08 | Pending |
| [10](10-active-scale-identity.md) | Normalize active-scale identity and remove duplicate state | 07 | Pending |
| [11](11-project-model-normalization.md) | Introduce `ProjectState` and grouped pitch state | 07, 10 | Pending |
| [12](12-workspace-settings-lifetime.md) | Move workspace preferences outside project and host state | 05, 11 | Pending |
| [13](13-final-cleanup-and-invariant-audit.md) | Remove obsolete paths and verify all architecture invariants | 06–12 | Pending |

Packages 03 and 04 may proceed in parallel after package 02. Package 08 may proceed in
parallel with packages 03–07 after package 02. All other ordering reflects data or API
dependencies.

## Global completion rules

Every package must:

- preserve unrelated user changes and avoid the frontend sibling unless explicitly
  included by the package;
- update explicit CMake source lists when files are added or removed;
- add or update focused tests for changed behavior;
- format only changed C++ files;
- build the narrowest affected target while iterating;
- pass `ctest --test-dir build --output-on-failure` before final completion unless a
  documented environment failure prevents it;
- leave no compatibility shim, temporary diagnostic, duplicated authority, or dead
  implementation made obsolete by that package.
