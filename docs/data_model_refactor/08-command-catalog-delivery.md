# 08: Immutable Command Catalog Delivery

## Objective

Keep backend command semantics authoritative while delivering one presentation-safe
catalog for frontend-local completion and documentation.

## Prerequisite

Package 02. This package may proceed in parallel with packages 03–07.

## Scope

- Finalize stable catalog metadata and its independent schema version.
- Include the immutable catalog in `session.hello`.
- Freeze registration before handshake.
- Remove `catalog.get` and per-keystroke backend completion endpoints.
- Remove post-construction registration if unused in production.
- Serialize target requirements, arguments, defaults, constraints, path, description,
  and existing aliases/grouping.
- Exclude handlers, C++ types, transaction details, and backend-only policy.
- Make command reference generation consume the same catalog representation.
- Implement frontend-local tolerant tokenization, filtering, ranking, and fuzzy search.

Frontend sibling work is explicitly part of this package.

## Acceptance criteria

- [ ] One catalog payload is delivered during session initialization.
- [ ] Catalog schema versioning is independent of project snapshot schema versioning.
- [ ] Frontend completion makes no per-keystroke backend request.
- [ ] Backend parsing and validation remain strict and authoritative.
- [ ] Only the active command-chain segment is tolerantly tokenized by the frontend.
- [ ] Command documentation and completion do not use divergent metadata sources.
- [ ] Removed endpoints and dead registration/completion code have no remaining callers.

## Verification

- [ ] Backend serialization tests cover complete stable metadata and schema version.
- [ ] Frontend tests cover partial input, chain segments, ranking, constraints, and
      catalog version handling.
- [ ] Affected backend/frontend tests and full backend `ctest` pass, or blockers are
      recorded.
