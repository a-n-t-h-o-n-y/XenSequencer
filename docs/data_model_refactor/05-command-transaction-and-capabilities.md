# 05: Command Transaction and Capability Contexts

## Objective

Stop copying complete `PluginState` per submission. Execute commands through lazy
resource candidates and narrowly typed capabilities.

## Prerequisites

Packages 02 and 04.

## Scope

- Add `CommandTransaction`, `HistoryPlan`, and submission effect tracking.
- Lazily copy only resources mutated by the chain.
- Read current const resources unless an earlier command created a candidate.
- Replace universal handler access with a small set of typed capability contexts.
- Route file reads/writes through declared transactional file capabilities.
- Move revision checks, target resolution, history planning, repeat recording, and
  candidate installation into the executor.
- Preserve the existing best-effort external-effect rollback contract.
- Enforce success ordering: validate, prepare, apply effects, install backend
  candidates, finalize effects, publish.
- Keep `PluginState` only as a temporary composition root.

## Acceptance criteria

- [x] Informational commands copy no project, library, workspace, or history state.
- [x] A normal project edit copies only the current project.
- [x] A library replacement copies only the library aggregate.
- [x] Handlers receive only capabilities declared by their command policy.
- [x] Handlers cannot access `PluginState`, timeline internals, history planning, or
      command-session authority.
- [x] One command chain installs at most one project-history transition.
- [x] Mixed file/domain operations retain rollback behavior and report rollback
      failures.
- [x] All potentially throwing validation/allocation occurs before no-fail backend
      installation.

## Verification

- [x] Tests cover read-only chains, project-only edits, library-only mutation, mixed
      chains, denied capabilities, effect failure, and rollback failure reporting.
- [x] Focused `XenTests` execution and full `ctest` pass, or blockers are recorded.
