# Command System And State Refactor Plan

Purpose: make command execution more explicit, deterministic, and idiomatic by separating parsing from execution, threading execution context through command chains, and centralizing commit/error policy.

## Goals

- [ ] Keep engine and editor/session concerns clearly separated.
- [ ] Replace ambient selection reads with explicit execution context.
- [ ] Preserve current behavior for key workflows (movement, edit commands, semicolon chains, `again`, undo/redo continuity).
- [ ] Make command execution policy explicit (chain semantics, error behavior, commit behavior).
- [ ] Keep migration incremental and compilable at each step.

## Current Baseline (Already True)

- [x] Command execution is centralized in processor (`src/xen_processor.cpp`).
- [x] State model is split between engine and editor session (`include/xen/state.hpp`).
- [x] Command definitions are centralized (`src/xen_command_tree.cpp`).

## Phase 0: Specification First

- [x] Write a short command execution spec in this document and keep it as source of truth.
- [x] Define the canonical command chain model:
  - left-to-right execution
  - per-command effective target
  - inherited context when no target override is given
  - command-local context updates flowing into next command
- [x] Define error policy explicitly:
  - partial apply vs transactional
  - message/status return policy
  - what is committed on failure
- [x] Define `again` behavior in terms of canonical command expansion.

## Phase 0 Draft Spec (Proposed)

This section is the proposed baseline for implementation. Any decisions marked in the decision checklist should be confirmed before coding begins.

### Terminology

- `raw command string`: user-provided text (command bar, keybinding, or UI emission).
- `command chain`: ordered list of command segments from a single raw command string.
- `command segment`: one command unit in a chain (split by top-level semicolons).
- `invocation`: parsed command id + typed args.
- `execution context`: non-engine runtime context used by command execution (target selection, input mode, command-session metadata).

### Parsing And Normalization

1. Parse raw command string into a command chain using syntax-aware semicolon splitting.
2. Semicolon splitting must ignore semicolons inside quoted strings and structured argument payloads.
3. Normalize whitespace per command segment after chain splitting.
4. Drop empty command segments.
5. Convert each command segment into an invocation (id + args).
6. Keep string parsing as adapter only; execution operates on invocation objects plus context.

### Chain Execution Model

1. Execution is strictly left-to-right.
2. Each invocation computes an effective target:
   - explicit target override in invocation, if present
   - otherwise inherited from current execution context
3. Each invocation returns:
   - updated engine state (if mutated)
   - updated execution context (if mutated)
   - status/result message
4. Context updates from one invocation are visible to subsequent invocations in the same chain.
5. Executor is the only component that owns chain orchestration, commit policy, and final status selection.

### Command Contract

1. Commands must be deterministic with explicit inputs (state + invocation + context).
2. Commands should report user-facing failures as `Error` status without throwing.
3. Throwing is reserved for invariant violations/internal faults.
4. Command handlers should not commit directly; they report mutation intent and rely on executor policy.

### Error And Commit Policy (Proposed)

1. Stop executing chain on first `Error` status.
2. Keep successful mutations from earlier commands in the same chain.
3. Commit engine mutations once at chain end (single commit boundary).
4. Context-only mutations should not create engine commits.
5. If an exception escapes command execution:
   - treat as hard failure
   - rollback staged engine changes to last commit point
   - return internal error status/message

### `again` Semantics (Proposed)

1. `again` expands to the most recently executed canonical command chain (already normalized and expanded).
2. Expansion happens at chain-execution level, not inside individual command handlers.
3. If no prior canonical chain exists, return an `Error` status with a clear message.
4. `again` expansion should be cycle-safe (prevent unbounded recursive expansion).

### Compatibility Expectations

1. Existing semicolon command workflows remain valid.
2. Selection-dependent workflows (`move ...; note ...`) remain valid via context carry-forward.
3. Behavior changes from current system must be intentional, documented, and covered by tests.

### Decision Checklist (Needs Confirmation)

- [x] Should chain execution stop on first `Error` (proposed) or continue and return final status?
- [x] Should `again` remember non-mutating command chains (proposed) or only mutating chains?
- [x] On hard failure, should editor/session context also rollback fully, or preserve some fields for continuity?

### Decision Outcomes (Confirmed)

1. Stop command-chain execution on first `Error`.
2. `again` remembers all non-empty canonical chains (including non-mutating chains).
3. Hard failures (exceptions) rollback both engine and editor/session context to last committed snapshot.

## Phase 1: Introduce Explicit Execution Context

- [x] Add a dedicated `ExecutionContext` type (target, input mode, optional command-session metadata).
- [x] Provide context construction from current editor state at command entry.
- [x] Thread context through executor loop for semicolon-separated chains.
- [x] Preserve current default behavior by mapping old ambient values to initial context.

## Phase 2: Decouple Parser From Execution

- [x] Keep command string parser as adapter only (string -> invocation objects).
- [x] Ensure semicolon splitting is syntax-aware (quoted strings and structured args are preserved).
- [x] Make invocation objects explicit enough for future typed commands.
- [x] Keep command bar simple (emit raw text + initial execution context from UI state).

## Phase 3: Convert Selection-Dependent Commands

- [x] Refactor selection-dependent helpers to take target/context explicitly instead of reading ambient `aux.selected`.
- [x] Update movement/select commands to return context updates that affect downstream commands in same chain.
- [x] Update sequence-index defaulting commands (`set sequence ...`) to use context-driven defaults.
- [x] Update special flows (`arp` chaining) to use explicit command-session/context state.

## Phase 4: Centralize Mutation And Commit Policy

- [x] Move commit decisions into one executor path rather than command-by-command commit handling.
- [x] Keep command handlers focused on pure state/context transforms + status.
- [x] Ensure undo/redo continuity semantics remain explicit and tested.
- [x] Remove duplicate/stale mutation logic after migration.

## Phase 5: Typed Action Layer (Optional But Recommended)

- [ ] Introduce typed actions and an `apply(action, engine, context)` style reducer/service.
- [ ] Keep string commands as an adapter to typed actions during migration.
- [ ] Gradually convert command handlers from ad-hoc lambdas to typed action dispatch.
- [ ] Minimize behavioral drift by running old/new paths against the same tests during transition.

## Testing And Validation

- [x] Add tests for chain target inheritance (`move right; note` style behavior).
- [ ] Add tests for per-command target override behavior (if supported in syntax).
- [x] Add tests for multi-command failure semantics and commit boundaries.
- [x] Add tests for `again` replay behavior with chained commands.
- [x] Add tests ensuring undo/redo preserves required editor-session continuity.
- [x] Add tests for parser correctness around semicolons inside quotes/structured args.

## Migration Rules

- [ ] No fallback paths or dual command systems beyond tightly scoped migration adapters.
- [ ] Fail fast on invalid command/context assumptions.
- [ ] Keep changes incremental and reviewable.
- [ ] Remove deprecated paths once replacement behavior is validated.

## Done Criteria

- [ ] No core command relies on ambient global selection state.
- [ ] Chain semantics are specified, implemented, and tested.
- [ ] Commit and error behavior are centrally enforced and predictable.
- [ ] Parser and executor responsibilities are clearly separated.
- [ ] Existing user-facing workflows remain intact or are intentionally versioned with documented changes.
