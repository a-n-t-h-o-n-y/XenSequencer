# Implementation Notes

This directory is for developer-facing notes that remain useful after feature work is
complete. The implementation and tests are authoritative when this summary drifts.

The current frontend migration contract is documented in
[`frontend_backend_contract.md`](frontend_backend_contract.md).

## Current architecture

- `CommandCatalog` owns parsing metadata, binding, policy, and handler registration.
  Its immutable presentation metadata is included in `session.hello`; command
  completion and UI navigation are frontend-local.
- Command submissions execute through lazy project, library, and workspace candidates.
  Effects are applied with rollback before backend candidates are installed.
- Project history identity and project revision are separate. Project-aware commands
  require the expected revision, and targeted commands also receive frontend-owned
  selection context.
- Project and library state use independent resources and revisions:
  - requests: `session.hello`, `state.get`, `command.execute`, and `library.get`;
  - events: `state.changed`, `library.changed`, `transport.phase.sync`, and
    `transport.stopped`.
- `session.hello` includes the command catalog and merged keymap. The old catalog,
  completion, and `keymap.get` requests are not supported.
- The project model has one top-level measure. Cells contain zero or more
  `MusicElement` values; an empty cell represents silence.

Useful implementation anchors:

- `src/webview_bridge.cpp`
- `src/bridge_serialize.cpp`
- `src/xen_processor.cpp`
- `src/command_transaction.cpp`
- `test/core/webview_bridge.test.cpp`
- `test/data_model_refactor.test.cpp`

## Known loose end

The bundled files in `data/demos` still use the removed sequence-bank and `Rest`
serialization schema. The current deserializer intentionally rejects that format, so
the demos need to be recreated or explicitly migrated before they are usable.
