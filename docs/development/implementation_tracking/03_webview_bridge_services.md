# Chunk 03: WebView Bridge Services

Breaking changes are fine. Prefer a clean bridge contract over preserving request
names or payload details. Do not add backwards-compatible aliases.

## Goal

Split the WebView bridge into pure protocol parsing/serialization, request
dispatch, and backend service calls so message contracts are easier to validate and
test.

## Implementable Work

- Replace the single `if/else` request chain in `WebviewBridge::handle_request_json`
  with a small dispatch table or command map.
- Extract pure DTO helpers for:
  - envelope parse/validate;
  - error envelope construction;
  - command context parse;
  - selection/keymap parse and serialization.
- Extract service-level handlers for session, project, command, library, and keymap
  requests. These should depend on the application service, not `XenProcessor`.
- Move filesystem-heavy library payload construction behind a library service seam.
- Keep all bridge errors structured; unknown request names, schema mismatches, and
  validation failures should return deterministic response envelopes.

## Frontend Notes

Update `../xen-frontend` in the same change if any request name, event name,
payload field, schema version, or error shape changes. Since breaking changes are
allowed, remove old frontend request handling instead of supporting both old and new
messages.

## Acceptance Criteria

- Bridge parser/serializer tests do not need a JUCE processor.
- Each bridge request handler can be tested independently with fake application
  services.
- `frontend_backend_contract.md` matches the implementation constants and test
  fixtures after the change.
