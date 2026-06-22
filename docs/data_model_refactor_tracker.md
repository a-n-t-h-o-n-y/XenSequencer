# Data Model and Command Architecture Refactor Tracker

This document tracks architectural improvements to the XenSequencer backend,
frontend boundary, command system, and project data model. It is intended to
remain current as decisions are made and implementation work lands.

Nothing here requires backward compatibility unless a migration is explicitly
chosen. Prefer one clear ownership model over parallel old and new paths.

## Goals

- Keep persisted, undoable musical state distinct from transient UI state.
- Make command inputs and effects explicit.
- Prevent stale frontend context from editing the wrong musical object.
- Preserve useful interactive workflows such as chord and arpeggio cycling.
- Avoid copying unrelated backend state to make a command submission atomic.
- Keep the backend authoritative for command definitions and execution while
  allowing the frontend to own presentation, navigation, search, and completion.
- Centralize model validation and reduce duplicated or contradictory state.

## Current codebase anchors

The recommendations below are grounded in the current implementation:

- `include/xen/state.hpp` combines `EngineState`, `EditorSessionState`,
  `ChordCycleState`, `ContentLibraryState`, and `PluginState`.
- `src/xen_processor.cpp::execute_command_string` copies the complete `PluginState`
  for submission atomicity and commits the timeline after the command chain.
- `include/xen/timeline.hpp` returns project states by value and exposes prospective
  commit IDs.
- `src/command_catalog_specs_transform.cpp` stores chord/arp baselines in editor state
  and compares them with `get_next_commit_id()`.
- `src/command_catalog_specs_set_and_shift.cpp` mutates both the active project scale
  and `ContentLibraryState::scale_shift_index`.
- `src/webview_bridge.cpp` exposes backend completion endpoints and separate
  `catalog.get`, while `src/bridge_serialize.cpp` combines project, editor, scales, and
  chords in one UI snapshot.

These are migration anchors, not intended final component boundaries.

## Proposed ownership model

### Backend-owned

- Persisted and undoable project state.
- Undo/redo history.
- Monotonic project revision and immutable history-entry identity.
- Strict command parsing, argument conversion, validation, binding, and execution.
- Command catalog definitions and metadata.
- Chord and arpeggio transform-cycle baselines.
- Command replay state used by `again`.
- Library and file access.
- Workspace settings such as current sequence and tuning directories.
- Publishing validated project snapshots to the audio thread.

### Frontend-owned

- Current selection/cursor.
- Input mode.
- Focus, routing, open panels, command-bar text, and command history presentation.
- Selection navigation.
- Command completion, filtering, fuzzy search, ranking, and display.
- Any other state that only controls how the user views or addresses the project.

The backend may return a suggested next selection after a structural edit, but the
frontend remains the owner of the active selection.

## Component model

- `ProjectState` models the persisted musical document used for rendering. It does not
  contain selection, history, library contents, command state, or transport.
- `ProjectHistory` models finalized project entries, an active preview, the undo/redo
  cursor, history-entry IDs, and project revisions. It does not perform command
  parsing or file IO and does not own UI state.
- `CommandSessionState` models transform baseline/cycle state and `again` replay state.
  It does not contain persisted project data or frontend focus/navigation.
- `ContentLibrary` models available scales, chords, measures, and tunings plus a
  library revision. It does not own active project choices, history, or frontend
  indexes.
- `WorkspaceSettings` models current library directories and other non-project backend
  preferences. It is not musical document data or undoable project state.
- `CommandCatalog` models immutable command syntax, semantic metadata, and binder
  definitions. It does not contain mutable execution/session state.
- `CommandTransaction` models one working project, transaction-local context, a
  command-session candidate, a history plan, and pending effects. It does not copy
  complete history or libraries.
- `AudioProjectSnapshot` is immutable render data derived from one validated project
  revision. It excludes editor, catalog, library, file, and history data.
- The frontend `ui` store owns selection, input mode, focus, panels, command text, and
  layout. It does not own authoritative project or library data.

The current `PluginState` is therefore a temporary composition root, not a domain
model. Its replacement may aggregate services, but commands should receive only the
specific component capabilities they declare.

## Command processing model

Use a reducer-like command pipeline internally, but do not adopt event sourcing.

The codebase already has most of the useful stages:

```text
command text
    -> parse CommandInvocation
    -> bind typed arguments and command definition
    -> apply to one working ProjectState
    -> validate
    -> install one history transaction
    -> publish snapshot and side effects
```

The refactor should make those stages and their data types clearer:

```cpp
struct CommandContext
{
    std::optional<SelectionPath> selection;
    std::optional<ProjectRevision> expected_project_revision;
};

struct CommandApplicationResult
{
    std::optional<SelectionPath> suggested_selection;
    CommandStatus status;
};

[[nodiscard]] auto apply(
    BoundCommand const &command,
    CommandApplicationContext &application) -> CommandApplicationResult;
```

`CommandApplicationContext` is a capability-specific view over the submission
transaction. A project edit receives the working project, resolved target, read-only
library access, command-session access when declared, and an external-effect sink. It
does not receive `PluginState`, `Timeline`, configuration, or unrelated services.

This is "reducer-like" because project edits are expressed as explicit inputs and
controlled effects rather than arbitrary access to all backend state. It does not
require every operation to return or copy a complete project independently; one
submission transaction owns one working project and applies the complete chain to it.

Do not introduce domain events such as `NoteCreated` or `NotesTransposed` unless a
concrete consumer requires them. Current snapshot-based history is simpler and already
supports undo/redo without replaying an event log. Event sourcing would add:

- event schema and migration work;
- replay and snapshot-compaction logic;
- random-operation determinism requirements;
- a second persistence representation beside the project file format.

Status messages and bridge notifications are outputs of command application, not a
persistent event log. Macro recording should continue to record parsed command
invocations, as `again` already does, rather than low-level mutation events.

- [ ] Introduce an explicit bound-command/application boundary.
- [ ] Keep one transaction for a complete command chain.
- [ ] Preserve parsed command invocations as the macro/replay representation.
- [ ] Do not add event persistence or replay-based history without a demonstrated need.

## 1. Move selection to frontend-owned command context

### Decision

Selection should not be stored in backend `PluginState`, engine history, or backend
UI snapshots. It should be sent as typed execution context with commands that need
a target.

Selection should not be appended to the textual command string. Text commands remain
human-readable and reusable; machine context travels beside them.

Example request shape:

```json
{
  "command": "duplicate",
  "context": {
    "selection": {
      "path": [
        { "kind": "element", "index": 0 },
        { "kind": "cell", "index": 2 }
      ]
    },
    "expected_project_revision": 42
  }
}
```

Possible C++ types:

```cpp
using ProjectRevision = std::uint64_t;

struct CommandContext
{
    std::optional<SelectionPath> selection{};
    std::optional<ProjectRevision> expected_project_revision{};
};

struct CommandResult
{
    CommandStatus status{};
    std::optional<SelectionPath> suggested_selection{};
};
```

`SelectedState` should be renamed to `SelectionPath` or `SelectionAddress`. It is an
address into project data, not independently meaningful state.

### Revision checking

A positional selection path can resolve to a different object after structural
changes. Every project-mutating request should therefore identify the project revision
against which it was produced. Target-dependent commands must additionally include a
selection. Informational commands may omit both.

The backend should reject an edit when:

- `expected_project_revision` is stale; or
- a required selection is absent; or
- the selection path does not resolve against the current project.

This is preferable to silently normalizing a stale path and editing a nearby object.
The response should return the current project snapshot/revision so the frontend can
reconcile.

Stable node IDs are not required initially. Positional paths plus revision checking
are sufficient for one local frontend. Stable IDs become useful if selections must
survive arbitrary remote changes, concurrent editors, or multiple independently
mutating views.

Do not use the existing timeline `commit_id` as the edit guard. It identifies a
history entry, but undo can make an older ID current again, and replaceable previews
need multiple distinct current states associated with one history position. This
creates an ABA problem for delayed requests.

Introduce two explicit concepts:

- `HistoryEntryId`: immutable identity of a finalized undo/redo entry.
- `ProjectRevision`: a monotonically increasing process-local revision that advances
  whenever the authoritative current project changes, including commit, preview
  replacement, undo, redo, load, and reset.

`expected_project_revision` uses `ProjectRevision`. Persisted project files do not need
to preserve it; loading installs project data and allocates a fresh revision.

Do not use the current `snapshot_version` unchanged as the edit guard. It advances for
any successfully submitted nonempty command, including informational commands, and
currently represents mixed UI publication ordering. Replace it with resource-specific
revisions. `ProjectRevision` orders project snapshots; equal revisions are valid for
idempotent responses, while lower revisions are stale.

The backend should reject stale commands rather than automatically rebase positional
selection paths. There is no generally correct rebase without stable node identity.

### Command-chain context

Commands in one submitted chain may need to observe selection changes produced by
earlier commands. Maintain a transaction-local context:

```text
request context
    -> command 1
    -> optional suggested selection becomes command 2's target
    -> command 2
    -> final suggested selection returned to frontend
```

This preserves behavior such as `move right; note 7` during migration, although
selection-navigation commands should ultimately move to the frontend.

### Structural edit selection suggestions

Domain edits should not directly take ownership of frontend state. They may return a
deterministic suggestion:

- `duplicate`: select the duplicate.
- `delete`: select the nearest surviving sibling or the parent cell.
- `lift`: select the lifted object at its new address.
- Pasting a cell over a selected element: select the replaced parent cell.
- Non-structural transforms: preserve the supplied selection.

The frontend may accept or ignore the suggestion.

### Migration plan

- [ ] Add `CommandContext` to the processor command-execution API.
- [ ] Add optional selection and expected project revision to `command.execute`.
- [ ] Introduce distinct `ProjectRevision` and `HistoryEntryId` types.
- [ ] Make target-dependent actions accept `SelectionPath` directly instead of
      `EditorSessionState`.
- [ ] Return an optional selection suggestion from structural commands.
- [ ] Carry context updates across commands in one submission.
- [ ] Move `move left/right/up/down` to frontend actions.
- [ ] Move input mode fully to frontend state.
- [ ] Remove `EditorSessionState` from `PluginState`, `EngineSnapshot`, and bridge
      snapshots.
- [ ] Remove backend editor-state snapshot version updates.

## 2. Model chord and arpeggio cycling as transform sessions

### Required behavior

Chord and arpeggio cycling must apply every candidate transform to the same original
musical material.

For example, given four notes at pitch zero:

```text
baseline:       0, 0, 0, 0
major result:   0, 4, 7, ...
minor result:   0, 3, 7, ...
```

The minor candidate must be built from the baseline, not from the already transformed
major result. Otherwise offsets accumulate and cycling is not a preview of alternative
chords.

This baseline is real command-session state and should remain backend-owned. It is not
frontend editor state.

### Problem with the current representation

`ChordCycleState` currently lives inside `EditorSessionState` and stores:

- a complete `EngineState` baseline;
- selection;
- a prospective commit ID;
- previous chord and inversion.

This has several drawbacks:

- Command execution state is mislabeled as editor state.
- Two complete engine baselines are retained for chord and arpeggio cycling.
- The state is included when the complete `PluginState` is copied transactionally.
- Correctness depends on `get_next_commit_id()` before the processor creates the
  commit.
- The current comparison is likely to restart a cycle across separate successful
  command submissions: after the first submission commits, the timeline's next commit
  ID advances beyond the stored ID.

The last point should be covered by processor-level tests before this area is changed.
The existing direct-handler test does not exercise processor commits between cycle
commands.

### Recommended model

Introduce backend command-session state separate from project history and frontend
state:

```cpp
enum class TransformKind
{
    Chord,
    Arpeggio,
};

struct TransformCycleSession
{
    TransformKind kind{};
    SelectionPath target{};
    TargetSnapshot baseline{};
    ProjectRevision result_revision{};
    std::string chord_name{};
    int inversion{-1};
};

struct CommandSessionState
{
    std::optional<TransformCycleSession> transform_cycle{};
    std::vector<CommandInvocation> repeat_chain{};
};
```

`TargetSnapshot` should normally contain only the selected cell or element, rather
than a complete `EngineState`. It should be a typed variant that records whether the
baseline is a cell or element; restoration must require the current target to have the
same kind. On each cycle:

1. Verify that command kind and selection match the active session.
2. Verify that the current project revision is the session's last result revision.
3. Restore the baseline target into a working copy of the current project.
4. Resolve the next chord/inversion.
5. Apply the transform to the restored baseline target.
6. Commit or publish the result.
7. Record the new result revision in the session.

Storing only the target is valid while continuation requires the current revision to
be exactly the prior preview result. No unrelated edit can have happened between
previews under that rule.

Start a new transform session when:

- no compatible session exists;
- the selection changes;
- chord and arpeggio command kinds differ;
- the current revision is not the previous cycle result;
- undo, redo, load, reset, or another project edit intervenes.

The binder should classify a transform as either `Start` or `Cycle`; handlers should
not infer lifecycle from raw command text. The exact treatment of explicit chord names
is retained as an open product decision at the end of this document.

### History semantics

Use a replaceable preview entry rather than one history entry per candidate:

```text
baseline commit -> active preview candidate
```

The first chord/arp transform begins a preview from the current finalized history
entry. A compatible cycle operation replaces the active candidate. It does not append
another undo step. Every replacement receives a fresh `ProjectRevision`; the finalized
history-entry identity remains unchanged until the preview is finalized as a new
entry.

The preview becomes a normal finalized history entry before a different project edit
or an operation that externalizes the current project, such as save/export. The new
edit is then applied after that finalized candidate. One undo of the later edit returns
to the selected chord/arp result; a second undo returns to the pre-transform baseline.

For save/export, "before" is logical transaction ordering: serialize and apply the
external effect from the candidate, then finalize the preview only after that effect
succeeds. A failed external effect leaves the preview active.

This should not be keyed literally on whether the submitted text is `again`.
`again` is expanded and rebound before execution. Continuation is determined from the
resolved operation:

- A compatible chord/arp cycle for the same target and transform kind continues and
  replaces the preview.
- This includes either a direct cycle command or `again` expanding to that operation.
- A transform classified as `Start` follows the explicit-start policy chosen below.
- A different project edit finalizes the preview before applying the edit.
- Informational commands and other operations that do not mutate or externalize the
  project may preserve the active preview.
- A failed command preserves the preview because no state or effects are installed.
- Undo while a preview is active cancels the preview and returns directly to its
  baseline. It does not first finalize the candidate and then undo it.
- Redo, reset, and load invalidate any active transform session. Their exact history
  behavior remains owned by the history operation itself.

Make this policy explicit rather than scattering command-name checks:

```cpp
enum class PreviewDisposition
{
    Preserve,
    Continue,
    Finalize,
    Cancel,
};
```

The command binder/executor can determine the disposition after `again` expansion and
argument binding. Chord/arp handlers still verify transform kind, target, and revision
before accepting `Continue`.

The history abstraction should expose a clear preview API:

```cpp
begin_preview(candidate);
replace_preview(candidate);
finalize_preview();
cancel_preview();
```

`begin_preview` retains the current finalized entry as the baseline and exposes the
candidate as the current project. `replace_preview` replaces only that candidate.
`finalize_preview` appends the current candidate as one finalized history entry.
`cancel_preview` discards it and restores the baseline. Each operation that changes
the current project allocates a fresh `ProjectRevision`.

Do not implement this by mutating timeline internals from command handlers. The
history abstraction owns preview state, history-entry IDs, and project revisions.

### Alternative considered: frontend-owned baseline

The frontend could send the original target with every candidate, but this would:

- duplicate domain data in UI state;
- allow untrusted or stale baseline payloads;
- complicate validation;
- make CLI or non-frontend command execution behave differently.

The frontend may choose the next chord/inversion, but the backend should retain and
validate the transform baseline.

### Migration and tests

- [ ] Add processor-level tests proving separate `chord` submissions use one baseline.
- [ ] Add equivalent arpeggio tests.
- [ ] Test selection changes and intervening edits start a new baseline.
- [ ] Test undo/redo/load/reset invalidate the active session.
- [ ] Test informational and failed commands preserve an active preview.
- [ ] Test a different project edit finalizes the candidate before its own commit.
- [ ] Test the chosen explicit chord/arp start policy.
- [ ] Move cycle state from `EditorSessionState` to `CommandSessionState`.
- [ ] Store only the selected target baseline where practical.
- [ ] Replace prospective commit-ID checks with actual project result revisions.
- [ ] Add replaceable preview support to the history abstraction.

## 3. Move command completion and search fully to the frontend

### Decision

The frontend should own completion and search. The backend should send authoritative
catalog metadata once during session initialization and should not answer per-keystroke
completion requests.

The backend must continue to own:

- strict parsing of submitted command text;
- argument conversion and validation;
- binding to executable command definitions;
- execution;
- authoritative catalog metadata.

Frontend completion is guidance, not validation. A suggestion being accepted by the
frontend never bypasses backend parsing and validation.

The catalog is the single source for command definitions, documentation generation,
frontend completion/search, and optional keybinding validation. The frontend may build
different indexes and presentation models from it, but should not maintain a separate
hard-coded command list.

### Why this boundary is cleaner

- Completion ranking and fuzzy search are presentation behavior.
- Per-keystroke bridge requests add latency and stale-response handling.
- The frontend can search descriptions, paths, argument names, and aliases using one
  local index.
- Completion UI can evolve without adding backend request shapes.
- The existing backend completion implementation rebuilds a completion tree for each
  request.
- The catalog already contains the information needed by the frontend.

### Catalog delivery

Include the complete catalog in `session.hello` so there is one initialization round
trip. Remove `catalog.get`; it has no distinct lifecycle or data source.

The session payload should include a catalog schema version independent of the project
snapshot schema:

```json
{
  "catalog_schema_version": 1,
  "commands": [
    {
      "path": ["set", "baseFrequency"],
      "description": "Set base frequency in Hz.",
      "accepts_pattern_prefix": false,
      "target": "none",
      "arguments": [
        {
          "name": "freq",
          "kind": "float",
          "display_type": "Float",
          "required": false,
          "default_text": "440",
          "constraints": {
            "minimum": 20,
            "maximum": 20000
          }
        }
      ]
    }
  ]
}
```

Prefer an explicit `required` field over inferring requiredness from whether a default
is null. Include as much semantic metadata as is reasonably defined by the command
catalog itself:

- command path and description;
- pattern-prefix support;
- target requirement;
- stable machine-readable argument kind;
- display type and argument name;
- required/default information;
- numeric ranges, enum values, or other constraints when the backend validator uses
  the same catalog definition;
- aliases if aliases are actually introduced into the command model;
- optional category/tags when they support command-palette grouping.

Constraints should only be exposed when they are catalog data shared with backend
validation. Do not duplicate validation constants independently in bridge serialization.
Do not expose internal handler details, transaction policy, or C++ type names merely
because they exist.

### Catalog immutability

The catalog is immutable for a running application version. Construct all commands
before frontend handshake and freeze the catalog. No `catalog.changed` event or catalog
revision is required.

Runtime registration currently exists as an API/test capability, but it should either
be restricted to catalog construction or removed if it has no production consumer.
The frontend receives one catalog snapshot for the session.

### Frontend completion behavior

The frontend can build a token trie or a flat search index from catalog metadata and
support:

- case-insensitive path-token completion;
- multiple candidates;
- fuzzy command and description search;
- argument hints and defaults;
- command palette ranking and recent-command weighting;
- aliases, if aliases are later added to catalog metadata;
- command-chain-aware completion by completing only the active segment.

The frontend does not need to reproduce the complete strict backend parser. It only
needs enough tolerant tokenization to identify the active command segment and token.
Malformed final input can still show best-effort suggestions; execution remains strict.

Pattern-prefix presentation should use `accepts_pattern_prefix`. Exact pattern syntax
continues to be validated by the backend.

### Backend cleanup plan

- [ ] Extend the catalog payload with explicit required/default metadata and stable
      argument kinds, target requirements, constraints, and optional grouping metadata.
- [ ] Include catalog metadata in `session.hello`.
- [ ] Freeze the catalog before handshake.
- [ ] Restrict or remove post-construction runtime registration.
- [ ] Implement frontend-local completion and search.
- [ ] Remove bridge endpoints:
  - `command.completeText`
  - `command.completeId`
  - `command.complete`
- [ ] Remove `CommandCatalog::complete_text`, `complete_id`, and `complete`.
- [ ] Remove `CompletionResult`, `CompletionCandidate`, and completion-only parser mode
      if no other caller needs them.
- [ ] Remove completion wrappers from `guide_text`.
- [ ] Remove completion-specific backend tests and replace them with catalog-schema
      tests plus frontend completion tests.
- [ ] Update bridge and frontend documentation.
- [ ] Remove `catalog.get`; `session.hello` is the sole catalog delivery path.

Backend documentation generation may continue to use catalog metadata. It is not the
same responsibility as interactive completion. Diagnostics can inspect or log the
catalog delivered by `session.hello`; a second endpoint would duplicate protocol
surface without a distinct lifecycle or data source.

Do not send a second command-reference representation in `session.hello`. If the
frontend needs command-reference pages, derive them from the delivered catalog. The
`reference` payload should contain only non-command reference material that is not
already represented by catalog or keymap metadata.

## 4. Stop copying complete backend state per submission

### Problem

The command executor currently copies `PluginState` to obtain atomic execution. That
copy includes:

- the complete timeline and every historical engine state;
- loaded scales and chords;
- editor and transform-cycle state;
- configuration.

Submission cost therefore grows with undo history and library size.

### Direction

Execute against a transaction containing only the mutable current project, command
session, and pending external effects:

```cpp
struct CommandTransaction
{
    ProjectState project;
    CommandContext context;
    CommandSessionState session;
    SubmissionEffects effects;
    HistoryPlan history;
};
```

`HistoryPlan` records the intended transition without mutating live history while
handlers run. It can represent no change, an ordinary commit, preview begin/replace/
cancel/finalize, or the atomic "finalize preview, then commit edit" transition needed
when a different project edit follows an active preview.

On success:

1. Validate the resulting project.
2. Prepare all external effects without making them visible.
3. Apply external effects using their existing rollback contract.
4. Install the planned history transition and command-session updates atomically.
5. Finalize external effects.
6. Publish the new project revision to audio and UI.

If effect application or backend installation fails, roll back applied effects and
leave project history, command-session state, and publication unchanged. The history
commit path should either be non-throwing after preparation or provide an equally
strong rollback guarantee.

The timeline object itself should not be copied to run a command.

The current processor is already submission-atomic: handlers mutate a copied working
state, and an error returns before that state is installed. External file effects also
have prepare/apply/rollback handling. Preserve this behavior. A bridge error response
should return the current authoritative snapshot, not describe or expose a partially
applied project.

Related improvements:

- `Timeline::get_state()` and `get_committed_state()` should return `State const &`.
- A caller intending to edit should explicitly copy only the current project.
- History commands should use a narrow history interface rather than a general command
  handler with access to all backend state.

- [ ] Introduce a current-project transaction independent of `Timeline`.
- [ ] Stop copying `PluginState` in `execute_command_string`.
- [ ] Return timeline state by const reference.
- [ ] Preserve transactional file-effect rollback.
- [ ] Benchmark submission cost with large history before and after.

## 5. Narrow command handler authority

`CommandExecutor` currently receives all of `PluginState`. Any command can modify
history, editor state, configuration, libraries, and project data.

Prefer capability-specific command types or a narrow execution facade:

```cpp
struct ProjectCommandContext
{
    ProjectState &project;
    std::optional<ResolvedTarget> target;
    ContentLibrary const &library;
    CommandSessionState &session;
    SubmissionEffects &effects;
};
```

Separate categories may include:

- Project edit commands.
- History commands.
- Library reload commands.
- File import/export commands.
- Informational commands.

The type system should make a command's allowed effects visible. This also removes
repeated `get_state`, mutate, and `stage` boilerplate.

Each bound command should carry policy metadata selected from closed enums rather than
handler-name checks:

```cpp
enum class CommandEffect
{
    Informational,
    ProjectEdit,
    History,
    LibraryMutation,
    ExternalizeProject,
};
```

Together, `CommandEffect`, `TargetRequirement`, `RepeatPolicy`, and
`PreviewDisposition` define revision requirements, available capabilities, replay
behavior, and preview lifecycle. Metadata exposed to the frontend should include only
the presentation-relevant subset; backend transaction policy remains internal.

- [ ] Define command effect categories.
- [ ] Replace direct `PluginState &` access with narrow contexts.
- [ ] Keep external writes in `SubmissionEffects`.
- [ ] Ensure runtime-added commands declare their effect category.

## 6. Remove mutable secondary truth for active scale

`ContentLibraryState::scale_shift_index` can disagree with `EngineState::scale` after:

- undo/redo;
- loading plugin state;
- `set scale`;
- library reload;
- direct scale replacement.

The active scale must have one source of truth.

Possible clean model:

```cpp
struct ActiveScale
{
    std::optional<std::string> source_id;
    Scale definition;
};
```

The embedded definition preserves project reproducibility if library files change.
`source_id` identifies the library entry from which the active definition was chosen;
it is provenance, not a second source of musical truth. Mode shifts and other project
edits update `definition` without changing the source identity. Scale-list cycling
locates `source_id` in the current library and fails clearly if it is absent; it must
not silently switch to an entry that merely has equal contents.

If library identity is not needed, remove the index and derive it from the active scale
definition every time. Do not keep both identity-based and definition-matching
behavior.

- [ ] Choose stable library scale identity or pure definition matching.
- [ ] Remove `scale_shift_index`.
- [ ] Define whether scale cycling requires stable source identity.
- [ ] Define behavior when the active scale's source is absent from the current library.
- [ ] Test scale cycling after undo, load, set, and library reload.

## 7. Rename and group project-domain state

`EngineState` is the persisted, undoable project/document rather than runtime engine
machinery. Rename it to `ProjectState` or `SequencerProject`.

Group coupled pitch configuration:

```cpp
struct NamedTuning
{
    std::string name;
    sequence::Tuning definition;
};

struct PitchSystem
{
    NamedTuning tuning;
    std::optional<ActiveScale> scale;
    int transposition{};
    TranslateDirection scale_translation{TranslateDirection::Up};
    float base_frequency{440.f};
};

struct ProjectState
{
    Measure measure;
    PitchSystem pitch;
};
```

This removes separate `tuning` and `tuning_name` fields and gives validation and
serialization a clearer boundary. `NamedTuning::name` is display metadata embedded in
the project, not a library lookup key. If tuning provenance is later required, model
it separately as an optional stable source ID.

- [ ] Rename `EngineState` after command-context work stabilizes.
- [ ] Group tuning name and definition.
- [ ] Group pitch-system settings.
- [ ] Update serialization as an intentional schema break.
- [ ] Keep audio-thread snapshots limited to data required for rendering.

## 8. Centralize project validation

Public aggregate types currently permit invalid combinations, and validation is spread
across commands and deserialization.

Add one authoritative project validation boundary:

```cpp
void validate(ProjectState const &project);
```

Run it:

- after deserialization;
- before history commit or preview installation;
- before publication to the audio engine;
- after importing a measure or tuning.

Commands that replace coupled data, such as tuning plus scale/key normalization, must
construct one complete candidate and validate at the transaction boundary. The model
should not publish intermediate invalid states.

Validation should cover at least:

- nonempty, finite tuning data;
- finite positive base frequency;
- valid time signature and maximum supported duration;
- scale validity against tuning length;
- valid key/transposition constraints;
- finite and supported cell weights;
- finite and supported velocity, delay, and gate values;
- recursive model invariants.

Selection validation belongs to command-context resolution because selection is not
part of project state.

- [ ] Define project-wide invariants.
- [ ] Add `validate(ProjectState const &)`.
- [ ] Remove duplicated command-local checks where the shared validator is sufficient.
- [ ] Keep user-facing command errors specific when validation can fail predictably.

## 9. Make target requirements explicit

`increment_state` infers whether an operation supports a cell or element from callable
signatures. This hides target requirements and produces runtime failures for some
selection kinds.

Prefer explicit operations:

```cpp
auto transform_cell(ProjectState, SelectionPath, CellTransform) -> ProjectState;
auto transform_element(ProjectState, SelectionPath, ElementTransform) -> ProjectState;
auto transform_target(ProjectState, SelectionPath, TargetTransform) -> ProjectState;
```

Represent requirements with a closed backend enum and serialize that same value into
catalog metadata:

```cpp
enum class TargetRequirement
{
    None,
    Cell,
    Element,
    CellOrElement,
};
```

Command catalog metadata should expose target requirements:

```json
{
  "target": "cell | element | cell-or-element | none"
}
```

That allows the frontend to explain unavailable commands, while backend resolution
remains authoritative.

Also rename misleading actions such as `delete_cell`, which can delete an element.

- [ ] Add explicit target requirement metadata.
- [ ] Replace `increment_state` callable introspection.
- [ ] Rename actions to match their actual semantics.
- [ ] Standardize target-resolution errors.

## 10. Separate project, library, and transport publication

The current UI snapshot combines project, editor, and the complete scale/chord library.
This causes unrelated data to be serialized and invalidated together.

Publish separate revisioned resources:

- Project snapshot, monotonic project revision, and optional history-entry identity.
- Library snapshot and library revision.
- Transport/phase events.
- Static command catalog for the session.

While a preview is active, the current project does not yet have a finalized
`HistoryEntryId`; publish that field as null. Add a separate preview-baseline ID only
if the frontend has a concrete use for it.

Potential bridge lifecycle:

```text
session.hello
    -> protocol versions
    -> command catalog
    -> keymap and non-command reference metadata

state.get
    -> current project snapshot/revision/history entry

library.get
    -> current library snapshot

state.changed
    -> project snapshot/revision/history entry only

library.changed
    -> library snapshot/revision only
```

### Frontend state ingestion

The sibling frontend should have one authoritative ingestion path for backend project
snapshots. `state.get`, `command.execute.snapshot`, and `state.changed` must all call
the same operation:

```ts
applySnapshot(snapshot)
```

That operation should:

- reject a snapshot with a lower `project_revision`;
- accept an equal revision only for an initial or idempotent response;
- replace the backend-owned project model atomically;
- invalidate or recompute derived read models;
- reconcile the frontend-owned selection against the new project.

React components should not call the native bridge directly. Put bridge IO behind a
typed client/service and expose application actions such as:

```ts
executeCommand(command, context)
getProject()
refreshLibrary()
```

This is a frontend architecture concern rather than a C++ model requirement, but it
prevents competing snapshot-update paths and keeps protocol details out of components.

Suggested frontend store split:

- `project`: latest accepted backend project snapshot, project revision, and optional
  history-entry identity.
- `session`: protocol, catalog, keymap, and non-command reference metadata.
- `library`: file/library view data, loading state, errors, and library revision.
- `ui`: selection, input mode, panels, focus, command bar, scroll, and zoom.
- `transport`: phase and active playback animation.

The recursive project tree should remain the canonical wire/project representation.
For rendering and interaction, derive indexes such as path lookup maps, flattened
visible rows, selected target resolution, and render geometry. Do not persist those
indexes as independent model truth.

Frontend-owned selection and command text may update optimistically because they are
local UI state. Project edits should normally reconcile from the backend response
snapshot rather than attempting to duplicate backend mutation logic in TypeScript.

- [ ] Remove editor state from project snapshots.
- [ ] Stop including full libraries in every project snapshot.
- [ ] Add independent project and library revisions.
- [ ] Keep transport synchronization independent.
- [ ] Route all frontend project snapshots through one `applySnapshot` path.
- [ ] Hide bridge calls behind a typed frontend client/service.
- [ ] Keep frontend tree indexes and render geometry derived from project snapshots.
- [ ] Reassess whether full project snapshots remain appropriate after the ownership
      split; retain them initially unless profiling justifies patches.

## Suggested implementation sequence

### Phase 1: establish terminology and protocol guards

- [ ] Add `ProjectRevision`, `HistoryEntryId`, and typed `CommandContext`.
- [ ] Pass selection with command execution.
- [ ] Return selection suggestions.
- [ ] Move navigation/input mode to frontend ownership.
- [ ] Add processor-level baseline-cycling tests.

### Phase 2: command transaction and history

- [ ] Execute against one current-project copy rather than copied history.
- [ ] Narrow command handler capabilities.
- [ ] Make target requirements and command effects explicit.
- [ ] Add centralized validation.
- [ ] Add replaceable preview support to history.
- [ ] Move chord/arp cycle data into backend `CommandSessionState`.

### Phase 3: frontend-local catalog use

- [ ] Finalize catalog metadata required for completion/search.
- [ ] Deliver and freeze the catalog during handshake.
- [ ] Implement frontend-local completion.
- [ ] Delete backend completion endpoints and implementation.

### Phase 4: normalize domain state

- [ ] Remove `scale_shift_index`.
- [ ] Rename/group project and pitch-system state.
- [ ] Separate project and library publication.

## Current decisions

- Project state is backend-owned, persisted, undoable, and validated as one aggregate.
- Selection and input mode are frontend-owned and are not part of project history.
- Project-mutating commands carry a monotonic expected `ProjectRevision`; positional
  selection paths are never silently rebased.
- `HistoryEntryId` and `ProjectRevision` are distinct concepts. `ProjectRevision`
  orders all project snapshot responses and events.
- Command definitions, binding, validation, and execution remain backend-owned.
- Completion, fuzzy search, ranking, and command presentation are frontend-owned and
  derive from one immutable catalog delivered during `session.hello`.
- Command handlers receive declared capabilities rather than mutable `PluginState`.
- A command chain is one transaction over one working project.
- Snapshot-based history remains the persistence/undo model; event sourcing is out of
  scope.
- Chord/arp cycling uses backend-owned target baselines and replaceable history
  previews, with a fresh project revision for every candidate.
- The active scale definition is persisted with the project; library position is not
  independent mutable truth.
- Project, library, session/catalog, and transport data are published as separate
  resources.

## Remaining questions

1. When an active chord/arp preview receives an explicit chord name, should it replace
   the current preview using the original baseline, or finalize the current candidate
   and start a new preview from that candidate?
2. What stable identity should library scales use across reloads: an explicit ID in
   YAML, a source-file-plus-entry ID, or normalized unique names? If no stable identity
   is added, should scale cycling fail when the exact active definition is absent?
3. Does `again` belong to `CommandSessionState` that resets on load/reset, or should
   repeat history survive those operations for the lifetime of the processor session?
4. Should `WorkspaceSettings` such as current library directories live only for the
   processor session, or persist as application-wide preferences outside project
   serialization?
