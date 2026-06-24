# Temporary Implementation Tracking

These files break the backend architecture cleanup into small implementable chunks.
They are temporary planning notes, not permanent architecture documentation.

Breaking changes are acceptable for every item here. Do not add backwards
compatibility shims, aliases, fallback behavior, or parallel old/new paths unless a
future task explicitly asks for a time-boxed migration.

Frontend impact is called out in each chunk. Update `../xen-frontend` alongside
the backend whenever the chunk changes bridge payloads, keymap semantics, command
behavior, or initialization expectations.

Chunks:

- `01_target_boundaries.md` - split CMake/library boundaries so JUCE adapters depend inward.
- `02_application_state_owner.md` - extract a state-owning application service from `XenProcessor`.
- `03_webview_bridge_services.md` - split bridge parsing, dispatch, payloads, and service calls.
- `04_realtime_midi_runtime.md` - move allocations/throwing work out of `processBlock`.
- `05_persistence_and_filesystem_ports.md` - consolidate file persistence and remove
  JUCE file leakage.
- `06_contract_tests_and_docs.md` - add contract-focused tests and fix drift in development docs.
