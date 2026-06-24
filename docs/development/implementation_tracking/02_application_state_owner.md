# Chunk 02: Application State Owner

Breaking changes are fine. Do not keep public mutable state or old processor
access paths for compatibility.

## Goal

Extract the application state and command execution ownership out of
`XenProcessor`, leaving the JUCE processor as an adapter around a testable backend
service.

## Implementable Work

- Introduce an application service, for example `SequencerSession` or
  `ApplicationController`, that owns `PluginState`, `CommandCatalog`,
  `WorkspaceSettingsStore`, command execution, project/library snapshots, and audio
  snapshot publication.
- Make `XenProcessor::plugin_state`, `pending_engine_state_update`, and
  `audio_thread_state_for_gui` private or move them behind narrow accessors.
- Keep `XenProcessor` responsible for JUCE lifecycle methods, state blob
  handoff, and audio callback wiring only.
- Update tests that directly mutate `processor.plugin_state` to use the new
  application service or intentional test helpers.
- Define one mutation path for project/library/workspace state; commands and bridge
  calls should go through the application service.

## Frontend Notes

No frontend payload changes are required by this chunk. If state revisions or
command results change shape while extracting the service, update
`../xen-frontend` and `frontend_backend_contract.md` in the same backend change.

## Acceptance Criteria

- Command execution can be tested without instantiating a JUCE `AudioProcessor`.
- `XenProcessor` no longer exposes mutable backend state publicly.
- Existing project revision and stale-command behavior remains covered by tests,
  even if APIs are renamed.
