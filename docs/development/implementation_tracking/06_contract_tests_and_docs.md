# Chunk 06: Contract Tests And Docs

Breaking changes are fine. Documentation should describe only the current contract;
do not document old payloads or compatibility behavior.

## Goal

Keep development docs, bridge constants, and tests aligned so frontend/backend
changes are discoverable and mechanically checked.

## Implementable Work

- Fix current bridge documentation drift:
  - catalog schema is `2`, not `1`;
  - catalog command payloads include `keywords`;
  - keymap events and payload examples should match implementation fixtures.
- Add checked-in representative JSON fixtures or snapshot-style tests for:
  - `session.hello`;
  - `state.get`;
  - `library.get`;
  - `command.execute`;
  - keymap get/set/remove/reset;
  - bridge error responses.
- Add tests that fail when documented schema constants and bridge constants diverge.
- Update `implementation_notes.md` after each architecture chunk so it stays a
  short current overview, not a stale migration note.
- Resolve the known stale demo-data note by either regenerating demos in the
  current schema or removing them from embedded runtime inputs.

## Frontend Notes

Any fixture or contract change here must be mirrored in `../xen-frontend` runtime
validators and bridge tests. Since breaking changes are allowed, update the
frontend to the new fixture shape directly instead of supporting old payloads.

## Acceptance Criteria

- `frontend_backend_contract.md` agrees with bridge constants and representative
  payload tests.
- Stale demo data is no longer listed as a known loose end.
- The docs explain current module responsibilities after the boundary refactors are
  done.
