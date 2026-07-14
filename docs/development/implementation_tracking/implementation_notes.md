# Implementation Notes

This directory is for developer-facing notes that remain useful after feature work is
complete. The implementation and tests are authoritative when this summary drifts.

The current frontend migration contract is documented in
[`frontend_backend_contract.md`](frontend_backend_contract.md).

## Current architecture

- `CommandCatalog` owns parsing metadata, binding, policy, and handler registration.
  Its immutable presentation metadata is included in `session.hello`; command
  completion and UI navigation are frontend-local.
- `SequencerSession` owns application state, command execution, workspace settings,
  and audio-project publication. `XenProcessor` is a JUCE adapter around realtime
  playback, plugin state serialization, and editor creation.
- Bridge protocol DTO parsing/serialization is separate from request dispatch.
  Session, project, command, library, keymap, and preferences requests route through
  bridge service seams so handlers can be tested without constructing the processor.
- Command submissions execute through lazy project, library, and workspace candidates.
  File effects use path-based read/write ports and the shared atomic text-write
  helper before backend candidates are installed.
- `WorkspaceSettings` stores plain filesystem paths. JUCE file objects are adapter
  details in stores, bridge library payloads, and host integration code.
- `MidiEngine` publishes validated render snapshots and uses bounded live-voice
  storage during playback. Invalid render inputs are rejected before publication.
- Project history identity and project revision are separate. Project-aware commands
  require the expected revision, and targeted commands also receive frontend-owned
  selection context.
- Project and library state use independent resources and revisions:
  - requests: `session.hello`, `state.get`, `command.execute`, and `library.get`;
  - events: `state.changed`, `library.changed`, `transport.phase.sync`, and
    `transport.stopped`.
- `session.hello` includes the command catalog and revisioned opaque keymap and
  preferences resources. They use whole-document read, write, and delete requests
  with optimistic revisions and independent change events.
- The project model has a reusable sequence bank and sparse composition arrangement.
  Cells contain zero or more `MusicElement` values; an empty cell represents silence.

Useful implementation anchors:

- `src/webview_bridge.cpp`
- `src/webview_bridge_protocol.cpp`
- `src/webview_bridge_services.cpp`
- `src/sequencer_session.cpp`
- `src/bridge_serialize.cpp`
- `src/midi_engine.cpp`
- `src/xen_processor.cpp`
- `src/command_transaction.cpp`
- `test/core/webview_bridge.test.cpp`
- `test/midi_engine.test.cpp`
- `test/sync/audio_thread_state_exchange.test.cpp`
- `test/data_model_refactor.test.cpp`
