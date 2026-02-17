# WebView Bridge Prep Refactor Plan

Purpose: clean up internals before implementing the JUCE 8 WebView bridge, with a focus on engine/UI separation and real-time safety.

## Priority 0: Correctness And RT Safety

- [x] Remove exceptions from audio thread path (`processBlock` must never throw).
  - Current issue: `src/xen_processor.cpp:55`
- [x] Replace unsafe cross-thread state handoff for non-trivial types.
  - Current risk: `LockFreeOptional<SequencerState>` with string/vector payloads.
  - Files: `include/xen/lock_free_optional.hpp:31`, `src/xen_processor.cpp:73`
- [x] Remove direct UI work from processor callbacks.
  - Current issue: UI alert in state save, direct editor update in state load.
  - Files: `src/xen_processor.cpp:126`, `src/xen_processor.cpp:146`

## Priority 1: Separate Engine And UI Responsibilities

- [x] Split state into explicit layers:
  - `EngineState` (sequencer/song/tuning/scale/key/base frequency)
  - `EditorSessionState` (selection/input mode/current panel/focus/command history)
  - `AppConfigState` (theme/library dirs/resources)
- [x] Remove UI/JUCE concerns from core state header.
  - Current coupling: `include/xen/state.hpp:12`, `include/xen/state.hpp:29`
- [x] Remove self-include in state header.
  - Current issue: `include/xen/state.hpp:32`

## Priority 2: Command Core Cleanup

- [ ] Introduce typed engine actions and reducer/service (`apply(action)`).
- [ ] Keep command-string parser as adapter only (string -> typed action).
- [x] Move UI-only commands out of engine command tree.
  - Current UI commands in core: `focus`, `show`, `theme`
  - File: `src/xen_command_tree.cpp:150`, `src/xen_command_tree.cpp:158`, `src/xen_command_tree.cpp:648`

## Priority 3: Bridge-Ready State Sync Boundary

- [x] Introduce engine->UI sync boundary now:
  - `EngineSnapshot` + `commitId`
  - optional incremental patch/events
- [x] UI consumes snapshots/events only; no direct mutation of engine internals.
- [ ] This boundary becomes the WebView bridge contract later.

## Implementation Notes

- Keep changes incremental and compilable at each step.
- Prefer adapters over rewrites:
  - old command system can continue while typed actions are introduced underneath.
- Avoid fallback compatibility paths; fail fast and keep architecture explicit.

## Change Log

- [x] Implemented safety + boundary baseline (snapshot versioning, mailbox handoff, UI command deprecations).
