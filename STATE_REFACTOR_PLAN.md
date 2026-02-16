# State Refactor Plan

This file tracks recommended state-model improvements before and during the WebView bridge migration.

## Status Legend
- `[ ]` not started
- `[-]` in progress
- `[x]` complete

## Backlog

### 1) Split state by responsibility
- Status: `[x]`
- Goal: Separate durable engine state, session/editor state, and UI/runtime wiring.
- Result: engine state now lives in `include/xen/engine_state.hpp`; runtime/editor state in `include/xen/runtime_state.hpp`; command routing split into engine/runtime trees.
- Key refs: `include/xen/engine_state.hpp`, `include/xen/runtime_state.hpp`, `src/engine.cpp`, `src/runtime_command_tree.cpp`

### 2) Define canonical bridge snapshot type
- Status: `[ ]`
- Goal: Add one transport DTO for UI sync (full-state in v1), including revision metadata.
- Current issue: only `SequencerState` is serialized today.
- Key refs: `include/xen/serialize.hpp:74`, `src/serialize.cpp:273`

### 3) Replace manual commit signaling with transaction API
- Status: `[ ]`
- Goal: Remove repetitive `stage(...) + set_commit_flag()` patterns with one mutation path.
- Current issue: commit policy is manual and easy to miss.
- Key refs: `include/xen/timeline.hpp:135`, `src/xen_command_tree.cpp`

### 4) Reduce implicit heavy copies of state
- Status: `[ ]`
- Goal: Add const-ref accessors for read paths and explicit cloning for mutation paths.
- Current issue: `Timeline::get_state()` returns by value, causing frequent large copies.
- Key refs: `include/xen/timeline.hpp:68`

### 5) Make command execution atomic by explicit policy
- Status: `[ ]`
- Goal: Define atomicity for `;` command chains and enforce it consistently.
- Current issue: rollback path is acknowledged as fragile.
- Key refs: `src/xen_processor.cpp:151`, `src/xen_processor.cpp:190`

### 6) Decouple UI effects from component IDs
- Status: `[-]`
- Goal: Replace string component-ID side effects (`focus/show`) with typed UI intents/events.
- Current issue: runtime commands still use string IDs, but they are now isolated to `RuntimeCommandTree` and removed from engine commands.
- Key refs: `src/runtime_command_tree.cpp`, `src/xen_command_tree.cpp`

### 7) Formalize persistence boundaries
- Status: `[ ]`
- Goal: Document and enforce what persists in DAW state vs runtime/session-only fields.
- Current issue: restore path mostly rehydrates sequencer state.
- Key refs: `src/xen_processor.cpp:132`

### 8) Clean up global/header coupling in state layer
- Status: `[-]`
- Goal: Remove self-include and move shared mutable runtime globals behind explicit context.
- Current issue: `state.hpp` is now a compatibility shim; runtime shared static still exists and should move behind explicit ownership.
- Key refs: `include/xen/state.hpp`, `include/xen/runtime_state.hpp`

## Notes
- This plan intentionally prioritizes state contract clarity over UI implementation details.
- We can add milestones and sequence these items once implementation starts.
- Pass 2 implemented the engine/runtime split and new command-path separation, and updated CMake target boundaries (`XenSequencerEngine` + `XenSequencerRuntime`).
