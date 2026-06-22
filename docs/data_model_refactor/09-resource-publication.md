# 09: Resource Publication and Frontend Ingestion

## Objective

Publish project, library, session, and transport as separate resources and route all
project snapshots through one revision-aware frontend ingestion path.

## Prerequisites

Packages 03, 05, and 08.

## Scope

- Implement the resource endpoints/events defined in the tracker.
- Publish project revision and current history entry ID with project snapshots.
- Add and advance `LibraryRevision` on every successful replacement/reload, including
  equal-value reloads.
- Keep transport/phase and audio render snapshots independent.
- Publish no project update for no-op transform candidates.
- Add a single frontend `applySnapshot` operation for all project snapshot sources.
- Put bridge IO behind a typed frontend client/service.
- Split frontend stores along project, session, library, UI, and transport ownership.
- Recompute derived read models and reconcile frontend-owned selection atomically.

Frontend sibling work is explicitly part of this package.

## Acceptance criteria

- [ ] `session.hello` contains session resources, not project/library state.
- [ ] `state.get` and `state.changed` contain only project resource data plus identity.
- [ ] `library.get` and `library.changed` contain only library resource data and
      revision.
- [ ] Frontend ingestion rejects lower revisions and permits equal revisions only for
      initial/idempotent responses.
- [ ] All snapshot sources use the same atomic ingestion operation.
- [ ] UI selection, input mode, focus, panels, command text, scroll, and zoom are not
      overwritten by backend project snapshots.
- [ ] Audio snapshots contain only validated immutable render data.
- [ ] Components no longer call the native bridge directly.

## Verification

- [ ] Backend tests cover resource payload boundaries and publication/no-publication
      rules.
- [ ] Frontend tests cover out-of-order, equal, and newer revisions plus selection
      reconciliation.
- [ ] Affected backend/frontend tests and full backend `ctest` pass, or blockers are
      recorded.
