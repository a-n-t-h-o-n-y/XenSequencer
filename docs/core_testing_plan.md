# Core Testing Plan

Purpose: establish a practical test baseline for core behavior before larger refactors.

This plan intentionally ignores/replaces the current unit test files in `test/`.

## Goals

- Verify core sequencing behavior after command execution.
- Verify engine/UI synchronization contracts introduced in the latest core changes.
- Catch regressions in state serialization and command-history flows.
- Keep tests fast enough to run locally before commits.

## Non-Goals

- No broad pixel-level GUI snapshot testing.
- No host-specific DAW automation in this phase.
- No compatibility shims for old architecture paths.

## Test Layers

1. **Core logic tests** (highest priority)
   - Scope: command tree behavior, timeline commit/undo/redo semantics, action invariants.
   - Dependencies: no editor UI.
   - Output: deterministic state assertions.

2. **Processor contract tests**
   - Scope: `XenProcessor::execute_command_string`, snapshot version updates, state round-trip.
   - Dependencies: processor + serialization path.
   - Output: contract assertions around commit IDs, messages, and state coherence.

3. **Mailbox and cross-thread boundary tests**
   - Scope: `EngineStateMailbox` publish/consume behavior and version monotonicity.
   - Dependencies: mailbox and `EngineState`.
   - Output: correctness under repeated publish/consume and skipped intermediate states.

4. **UI adapter smoke tests**
   - Scope: command bar input flow and plugin window update wiring with `EngineSnapshot`.
   - Dependencies: JUCE GUI init.
   - Output: low-count smoke checks (not exhaustive GUI tests).

## New Test Layout

Replace ad-hoc file placement with the following structure:

```text
test/
  core/
    command_tree.test.cpp
    timeline_commit.test.cpp
    state_actions.test.cpp
  processor/
    processor_commands.test.cpp
    processor_state_roundtrip.test.cpp
  sync/
    engine_state_mailbox.test.cpp
    engine_snapshot_contract.test.cpp
  ui_smoke/
    command_bar_smoke.test.cpp
    plugin_window_update_smoke.test.cpp
  support/
    fixtures.hpp
    temp_files.hpp
```

## High-Value Cases (First Pass)

### A) `EngineStateMailbox`

- `publish` increments version from `0 -> N`.
- `try_consume_latest` returns `false` when no new version exists.
- After multiple publishes before consume, consumer receives only the latest snapshot.
- Consumed snapshot equals published `EngineState` (including string/vector payload fields).

### B) Processor command contracts

- `execute_command_string("set key 5")` updates timeline commit state and snapshot version.
- `execute_command_string("again")` replays previous command text.
- Invalid command returns error level without crashing.
- Deprecated UI commands (`focus`, `show`, `set theme`) return warning and do not mutate engine state.
- Multi-command string (`a;b;c`) applies in order and reports final command status.

### C) State round-trip

- `getStateInformation` + `setStateInformation` preserves engine sequence/tuning/key/base frequency.
- Invalid serialized payload in `setStateInformation` fails safely (no throw escaping API boundary).

### D) Timeline semantics

- Mutating commands set commit flag and become undoable.
- Non-mutating commands do not create timeline commits.
- Undo/redo preserves `EditorSessionState` selection continuity rules currently implemented.

### E) UI smoke

- `PluginWindow::update(snapshot, scales)` updates visible components without exceptions.
- Command bar emits command and close-request flow without deadlock or recursion.

## Build And Execution Plan

### Phase 0: Harness reset

- Keep `XEN_BUILD_TESTS=ON` path.
- Replace current `XenTests` source list with the new folder structure above.
- Add `ctest` registration for each test executable.

Exit criteria:
- Clean configure/build and all new tests runnable through one command.

### Phase 1: Core + Sync baseline

- Implement `core/` and `sync/` tests first.
- Add fixtures for deterministic `PluginState` and temporary filesystem resources.

Exit criteria:
- Core command/timeline and mailbox contracts covered.

### Phase 2: Processor contracts

- Add processor command tests and serialization round-trip tests.
- Gate critical API contracts around snapshot/version increments.

Exit criteria:
- Regressions in latest core boundary changes are caught automatically.

### Phase 3: UI smoke

- Add minimal JUCE-initialized smoke tests for command bar and plugin window update flow.
- Keep test count small and behavior-focused.

Exit criteria:
- Basic UI adapter wiring is continuously verified.

## Suggested Local Gate

Before merging core or refactor work, run:

```bash
cmake -S . -B build -DXEN_BUILD_TESTS=ON
cmake --build build --target XenTests
ctest --test-dir build --output-on-failure
```

## Policy For Future Changes

- New core behavior requires tests in `core/`, `sync/`, or `processor/` before merge.
- Refactors touching engine/UI boundary require at least one snapshot/mailbox contract test update.
- Deprecated command behavior remains explicit and tested until commands are fully removed.
