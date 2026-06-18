The refactor is broadly sound, but I would not sign it off without cleaning up several ownership and failure-semantics issues.

## Findings

1. Deferred work can be silently lost after an exception

The exception path calls `reset_stage()`, which restores the last committed state—not the state at the beginning of this command submission: [xen_processor.cpp](/home/anthony/Documents/cpp/XenSequencer/src/xen_processor.cpp:409).

Sequence:

```text
set weights 0.5   # deferred, intentionally staged
paste             # throws because clipboard is empty
```

The paste failure discards the previously deferred weights. Bind errors and returned `MessageLevel::Error` do not do this, so the three error mechanisms have inconsistent rollback behavior.

Capture the pre-execution staged state and restore that on failure, or preferably remove long-lived staging across command submissions.

2. `again` stores executable closures instead of commands

`previous_command_chain_` stores `BoundCommand`: [xen_processor.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/xen_processor.hpp:98). These contain copied handlers and parsed arguments rather than a command AST or normalized input.

Consequences:

- Failed or partially executed chains replace the repeat target.
- Replay semantics depend on previously bound implementation objects.
- The “recursive expansion” limit is misleading; stored replay chains never contain replay controls.
- `BoundCommand` permits the invalid combination `Execute` plus an empty executor: [command_catalog_types.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/command_catalog_types.hpp:53).

For the `.` binding, I recommend Vim-style semantics: repeat the last successfully completed, repeatable edit. Store parsed invocations, then bind them normally when repeated. Exclude `again`, `commit`, `undo`, `redo`, informational commands, and navigation from replacing the repeat target.

Use a variant such as:

```cpp
using BoundStep = std::variant<ExecutableCommand, RepeatPrevious>;
```

That removes nullable executors and the separate control enum.

3. `CommandHistory` is dead backend state

`CommandHistory` is stored in `PluginState`, but nothing uses it: [state.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/state.hpp:154). Its source is still built, and there are no tests.

It also has questionable traversal semantics:

- Adding while browsing truncates history.
- `resize(++current_index_)` retains one future entry unexpectedly: [command_history.cpp](/home/anthony/Documents/cpp/XenSequencer/src/command_history.cpp:15).
- There is no preservation of the user’s current draft while browsing.

Command-line history is UI/session behavior and should probably live in the frontend. Conventional behavior is append-only submission history, including failed submissions, with a separately preserved draft. Remove the backend class unless backend persistence is specifically required.

Keep submission history and `.` repeat state separate; they have different policies.

4. Editor session state has three competing owners

`EditorSessionState` is:

- Stored inside every timeline entry.
- Passed separately as `ExecutionContext`.
- Re-staged by the DSL, many handlers, and the processor.

There are roughly forty explicit `state.aux = context` assignments, plus wrapper/finalization copies. Undo then manually restores only selection and input mode: [command_catalog_specs_bootstrap.cpp](/home/anthony/Documents/cpp/XenSequencer/src/command_catalog_specs_bootstrap.cpp:57).

This is the largest structural smell. Make the undo timeline contain `EngineState` only, and keep `EditorSessionState` separately in `PluginState`. Then:

```text
PluginState
├── Timeline<EngineState>
├── EditorSessionState
├── AppConfigState
└── ContentLibraryState
```

That removes manual undo preservation and most context synchronization code.

5. Commit intent is a transient side channel in `PluginState`

The DSL resets `state.commit_intent`, handlers mutate it, and the DSL reads it back: [command_dsl.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/command_dsl.hpp:353).

Execution protocol state should not be application state. Have handlers return a structured outcome containing status and commit policy, or declare the policy in the command definition.

The `commit` command also does not commit at its position in a chain—it merely requests one final commit after the entire chain. It can append duplicate timeline entries because `Timeline::commit()` always appends: [timeline.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/timeline.hpp:46).

Define explicitly whether:

- A chain is one atomic undo unit.
- `commit` is an immediate barrier.
- No-op commits create undo entries.

I recommend one successful submitted chain = one undo entry, no-op commits do nothing, and malformed chains are fully bound before execution.

6. Parsing accepts syntax its contract says it rejects

`split_input()` documents that unterminated quotes are rejected: [command.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/command.hpp:37). The tokenizer never checks final quote, escape, or structured-block depth: [string_manip.cpp](/home/anthony/Documents/cpp/XenSequencer/src/string_manip.cpp:236).

There are currently three separate syntax-aware passes—chain splitting, whitespace normalization, and tokenization—with slightly different quote/escape handling. Replace these with one lexer producing tokens/segments and explicit parse errors.

## Small leftovers

- `is_again_invocation()` is unused outside its test and predates catalog-based replay: [command.cpp](/home/anthony/Documents/cpp/XenSequencer/src/command.cpp:55).
- The old commented-out tokenizer remains in [string_manip.cpp](/home/anthony/Documents/cpp/XenSequencer/src/string_manip.cpp:301).
- `load keys` remains as a deprecated no-op despite compatibility not being required.
- `complete_text` and `complete_id` are documented as compatibility projections; remove them if the frontend has migrated to structured completion.

## Recommended final shape

```text
raw input
  → single lexer/parser
  → parsed command chain
  → expand repeat from last successful repeatable AST
  → bind entire chain
  → execute with one explicit transaction
  → commit changed engine state once
  → update repeat target on success
```

Submission history should be frontend-owned and append-only. Repeat state should be backend-owned and contain parsed commands, not executor closures.

Verification: the worktree was clean and `ctest --test-dir build --output-on-failure` passed all tests. No files were changed.
