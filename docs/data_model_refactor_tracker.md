# Data Model and Command Architecture Refactor

This document records the target architecture and implementation plan for the
XenSequencer backend, frontend boundary, command system, and project model.

Breaking changes are acceptable. Prefer one clear ownership model over compatibility
paths or parallel old/new behavior.

## Goals

- Separate persisted musical state from UI and command-session state.
- Make command inputs, resource access, history behavior, and effects explicit.
- Reject stale positional selections before they edit the wrong object.
- Preserve chord/arpeggio cycling without accumulating transforms or undo entries.
- Keep command execution atomic without copying history and unrelated resources.
- Keep backend command definitions authoritative while the frontend owns presentation.
- Centralize model validation and remove duplicated state.

## Non-negotiable invariants

- No project-aware command runs without an expected current `ProjectRevision`.
- Stale positional selections are rejected, never rebased.
- Command handlers receive only declared capabilities.
- A command chain installs at most one project-history transition.
- Chord/arp cycling applies a backend-owned baseline and amends at most one committed
  entry.
- Project, library, command-session, UI, and transport state have separate ownership.

## Current codebase anchors

- `include/xen/state.hpp` combines project, editor, transform-cycle, library, and
  workspace state in `PluginState`.
- `src/xen_processor.cpp::execute_command_string` copies the complete `PluginState`
  for each submission and commits the timeline after a command chain.
- `include/xen/timeline.hpp` returns states by value and exposes prospective commit IDs.
- `src/command_catalog_specs_transform.cpp` stores complete chord/arp baselines in
  editor state and compares them with `get_next_commit_id()`.
- `ContentLibraryState::scale_shift_index` duplicates active-scale state.
- `src/bridge_serialize.cpp` publishes project, editor, scales, and chords together.
- `src/webview_bridge.cpp` exposes per-keystroke completion and a separate catalog
  endpoint.

These are migration anchors, not intended final boundaries.

## Target ownership and components

### Backend-owned

- Persisted, undoable `ProjectState`.
- `ProjectHistory`, including undo/redo, entry IDs, and project revisions.
- Strict command parsing, binding, validation, and execution.
- Immutable `CommandCatalog`.
- `CommandSessionState` for transform cycles and `again`.
- `ContentLibrary` and file access.
- Application-wide `WorkspaceSettings`.
- Immutable project snapshots published to the audio thread.

### Frontend-owned

- Selection/cursor and input mode.
- Focus, routing, panels, command text, and presentation history.
- Selection navigation.
- Completion, fuzzy search, ranking, and command presentation.
- Derived tree indexes, geometry, and other view-only state.

The backend may suggest a selection after a structural edit, but the frontend owns the
active selection.

### Components

- `ProjectState`: persisted musical document used for rendering. It contains no
  selection, history, library, command-session, workspace, or transport state.
- `ProjectHistory`: committed project entries, current-entry amendment, undo/redo
  cursor, `HistoryEntryId`, and `ProjectRevision`.
- `CommandSessionState`: transform-cycle baseline and `again` replay state.
- `ContentLibrary`: available scales, chords, measures, and tunings plus
  `LibraryRevision`.
- `WorkspaceSettings`: non-project paths and preferences persisted application-wide.
- `CommandCatalog`: immutable syntax, semantic metadata, and binder definitions.
- `CommandTransaction`: transaction context, history plan, command-session candidate,
  pending effects, and lazy candidates for resources mutated by a submission.
- `AudioProjectSnapshot`: validated immutable render data for one project revision.

`PluginState` is a temporary composition root, not a domain model. Commands must
receive only declared capabilities.

## Command processing

Use a reducer-like pipeline, not event sourcing:

```text
command text
    -> parse CommandInvocation
    -> bind typed arguments and command definition
    -> check request context and resolve targets
    -> apply through declared resource capabilities
    -> validate changed candidates
    -> install one submission transaction
    -> publish changed resources and side effects
```

Keep snapshot-based history. Macro/replay state records parsed command invocations, not
low-level domain events.

### Command context and result

```cpp
struct CommandContext
{
    std::optional<SelectionPath> selection;
    std::optional<ProjectRevision> expected_project_revision;
};

struct CommandApplicationResult
{
    CommandStatus status;
    std::optional<SelectionPath> suggested_selection;
};
```

### Command policy

Resource access is orthogonal. A single effect enum cannot model commands such as
`cut` (project edit plus file write), `paste` (project edit plus file read), or
`load measure` (project edit plus workspace and file reads).

```cpp
enum class ProjectOperation
{
    None,
    Read,
    Edit,
    ReplaceHistory,
    NavigateHistory,
};

enum class LibraryAccess
{
    None,
    Read,
    Mutate,
};

enum class WorkspaceAccess
{
    None,
    Read,
    Mutate,
};

enum class FileAccess
{
    None,
    Read,
    Write,
};

enum class TargetRequirement
{
    None,
    Cell,
    Element,
    CellOrElement,
};

enum class RepeatPolicy
{
    Never,
    OnSuccessfulProjectChange,
};

enum class HistoryPolicy
{
    None,
    Commit,
    AmendCompatibleTransform,
};

struct CommandPolicy
{
    CommandPolicy() = delete;

    ProjectOperation project;
    LibraryAccess library;
    WorkspaceAccess workspace;
    FileAccess files;
    TargetRequirement target;
    RepeatPolicy repeat;
    HistoryPolicy history;
};
```

Catalog construction must explicitly initialize every field and reject incoherent
combinations:

- A target requires at least `ProjectOperation::Read`.
- `Commit` and `AmendCompatibleTransform` require `ProjectOperation::Edit`.
- `ReplaceHistory` and `NavigateHistory` require `HistoryPolicy::None`.
- File and mutable-resource capabilities must match the handler context.

Representative policies:

| Command | Project | Library | Workspace | Files | History |
| --- | --- | --- | --- | --- | --- |
| `version` | `None` | `None` | `None` | `None` | `None` |
| `chord` / `arp` | `Edit` | `Read` | `None` | `None` | `AmendCompatibleTransform` |
| `cut` | `Edit` | `None` | `None` | `Write` | `Commit` |
| `paste` | `Edit` | `None` | `None` | `Read` | `Commit` |
| `load measure` | `Edit` | `None` | `Read` | `Read` | `Commit` |
| `save measure` | `Read` | `None` | `Read` | `Write` | `None` |
| `load chords` | `None` | `Mutate` | `None` | `Read` | `None` |
| `undo` / `redo` | `NavigateHistory` | `None` | `None` | `None` | `None` |
| full project load | `ReplaceHistory` | `None` | `Read` | `Read` | `None` |

The executor, not handlers, owns:

- expected-revision checks;
- target resolution;
- capability construction;
- repeat recording;
- history planning;
- command-session lifecycle;
- validation, installation, and publication.

For a command chain:

- Validate the expected revision before executing any project-aware command.
- Resolve targets against the transaction-local project and selection.
- Commit at most once. Only a chain containing exactly one compatible transform may
  amend the current entry.
- Update `again` only from qualifying commands that actually changed the project.
- Invalidate transform state on history navigation, library mutation, project
  replacement, or an intervening edit.
- Clear all command-session state on full project replacement.

Handlers receive typed capability contexts. They never receive `PluginState`,
`Timeline`, history planning, or transform-session authority.

## Selection and project revisions

Rename `SelectedState` to `SelectionPath` or `SelectionAddress`; it is an address into
project data, not independently meaningful state.

Selection travels beside command text:

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

Any command that reads, edits, replaces, or navigates project state requires the
revision against which it was produced. Targeted commands also require a selection.
Reject the request when the revision is stale, the selection is missing, or the path
does not resolve.

Do not rebase positional paths. Stable node IDs are unnecessary for one local frontend
while revision checking exists; add them only if selections must survive concurrent or
independent mutations.

### Revision and history identity

- `HistoryEntryId` is the immutable identity of one undo/redo entry.
- `ProjectRevision` is a monotonic process-local generation that advances whenever the
  authoritative current project changes.

Do not use timeline `commit_id` or the current mixed `snapshot_version` as the edit
guard. An amended entry has one entry ID but multiple revisions, and undo can return to
an older entry ID.

Rules:

- Effective commit, amendment, undo, redo, or project-context replacement advances the
  revision.
- Failed and equal-project operations do not.
- Host-state restoration allocates a fresh revision even for equal data, making
  requests from the previous context stale.
- Persisted project files do not store process-local revisions.
- Internal startup/test callers use the same typed API with the current revision.
- Host restoration uses a separate history-replacement API.

### Selection suggestions

Structural edits may return deterministic suggestions:

- `duplicate`: the duplicate.
- `delete`: nearest surviving sibling or parent.
- `lift`: the moved object at its new path.
- Pasting a cell over an element: the parent cell.
- Non-structural transforms: the supplied selection.

Within one chain, a suggestion may become the next command's target. Navigation and
input mode should ultimately move fully to frontend actions.

## Chord and arpeggio transform sessions

Every chord/arp candidate must be applied to the same original target:

```text
baseline:  0, 0, 0
major:     0, 4, 7
minor:     0, 3, 7
```

The baseline is backend command-session state, not editor or frontend state.

```cpp
enum class TransformKind
{
    Chord,
    Arpeggio,
};

struct TransformCycleSession
{
    TransformKind kind;
    SelectionPath target;
    TargetSnapshot baseline;
    std::optional<HistoryEntryId> candidate_entry_id;
    ProjectRevision result_revision;
    LibraryRevision library_revision;
    std::string chord_name;
    int inversion{-1};
};

struct CommandSessionState
{
    std::optional<TransformCycleSession> transform_cycle;
    std::vector<CommandInvocation> repeat_chain;
};
```

`TargetSnapshot` is a typed cell-or-element snapshot, not a complete project.

To continue a session:

1. Command kind and target must match.
2. Current project revision must equal `result_revision`.
3. Current entry must match `candidate_entry_id` when one exists.
4. Library revision must match.
5. Restore the baseline, resolve the next candidate, and apply it.

Start a new session after target/kind changes, an intervening project edit, library
reload, undo/redo, reset, load, or project replacement.

### History semantics

The first changed candidate commits a normal entry. Compatible candidates amend it:

```text
E41: baseline
E42: major
E42: minor
E42: minor inversion 1
```

E42 keeps its `HistoryEntryId`; each changed candidate receives a fresh
`ProjectRevision`. A no-op candidate keeps the transform session but creates no entry,
revision, or `again` target. A later changed candidate creates the first entry.

A different edit appends E43 and ends the session. Undo then returns to final E42 and
the next undo to E41. Redo restores the final E42 snapshot but not its transform
session.

Save/export reads the current committed candidate and does not finalize or alter
history. Failed and informational commands preserve the session when they change
neither project nor library.

`again` is expanded and rebound before compatibility is evaluated. It may amend only
when its expanded chain is exactly one compatible transform.

History needs narrow operations:

```cpp
commit(project);
amend_current(expected_entry_id, project);
replace_history(project);
```

`amend_current` requires the expected entry to be current and at the history tip. It
must not amend an older entry or implicitly truncate redo.

`replace_history` installs a new root, clears undo/redo, allocates new entry/revision
identity, and clears command-session state. Use it for host restoration and full
out-of-band replacement. User `reset` may remain an ordinary undoable edit but still
clears command-session state.

## Command catalog and frontend completion

The backend remains authoritative for command definitions, parsing, binding, argument
conversion, validation, and execution. The frontend owns completion, filtering,
ranking, fuzzy search, and display.

Deliver one immutable catalog in `session.hello`; remove `catalog.get` and per-keystroke
completion endpoints. Give the catalog payload its own schema version, independent of
the project snapshot schema.

Catalog metadata should include:

- path, description, and pattern-prefix support;
- target requirement;
- stable argument kind and display name;
- explicit required/default information;
- constraints shared with backend validation;
- aliases or grouping metadata only if those features exist.

Do not expose handler, transaction, C++ type, or backend-only policy details. Do not
send a second command-reference representation; derive command documentation from the
catalog.

The frontend needs tolerant tokenization only for the active command-chain segment.
Backend parsing remains strict.

Freeze catalog registration before handshake. Remove post-construction registration if
it has no production consumer.

## Transaction and handler design

Copying `PluginState` currently copies complete history, libraries, editor state, and
configuration. Replace it with lazy resource candidates:

```cpp
struct CommandTransaction
{
    CommandContext context;
    CommandSessionState session;
    SubmissionEffects effects;
    HistoryPlan history;
    std::optional<ProjectState> project;
    std::optional<ContentLibrary> library;
    std::optional<WorkspaceSettings> workspace;
};
```

Create a candidate only for a resource that a command mutates. Read-only access uses
the current const resource unless an earlier command in the chain created a candidate.
Thus informational commands copy no domain state, normal edits copy only the current
project, and library reloads copy only the library aggregate.

`HistoryPlan` represents no change, commit, or guarded amendment. Handlers cannot
request history transitions.

Success ordering:

1. Validate project/library/workspace candidates.
2. Prepare external effects.
3. Apply effects using the existing rollback contract.
4. Install all backend candidates through a no-fail commit path.
5. Finalize effects.
6. Publish changed resource revisions.

Filesystem replacement across multiple paths is not truly atomic. Keep best-effort
rollback and report rollback failures. Perform validation and potentially throwing
allocation before applying effects so backend installation cannot fail afterward.

`Timeline::get_state()` and `get_committed_state()` should return const references.
Callers explicitly copy the current project only when editing.

### Narrow handler contexts

Contexts compose declared capabilities, for example:

```cpp
struct ProjectCommandContext
{
    ProjectState &project;
    std::optional<ResolvedTarget> target;
    ContentLibrary const &library;
};

struct ExternalizeCommandContext
{
    ProjectState const &project;
    SubmissionEffects &effects;
};
```

Use a small set of typed adapters for combinations that exist. Do not create a
universal context with runtime access checks.

Transform handlers receive an already restored target and only apply the transform.
File reads and writes go through declared transactional file capabilities.

## Project and library model

### Active scale

Remove `ContentLibraryState::scale_shift_index`. Persist the active definition with
optional provenance:

```cpp
struct ActiveScale
{
    std::optional<std::string> source_id;
    Scale definition;
};
```

`definition` is musical truth. `source_id` identifies the library entry used to choose
it and remains unchanged by mode shifts.

Library scales require unique stable YAML IDs:

```yaml
- id: xen.major-diatonic
  name: Major Diatonic
  tuning_length: 12
  intervals: [2, 2, 1, 2, 2, 2, 1]
```

Scale cycling requires a source ID present in the current library and fails clearly
otherwise. Embedded definitions remain playable. Do not infer identity from names or
content; projects without an ID must explicitly select a library scale before cycling.

### Project grouping

Rename `EngineState` to `ProjectState` or `SequencerProject` and group coupled pitch
state:

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
    int transposition;
    TranslateDirection scale_translation{TranslateDirection::Up};
    float base_frequency{440.f};
};

struct ProjectState
{
    Measure measure;
    PitchSystem pitch;
};
```

Tuning name is embedded display metadata, not a library lookup key. Treat
serialization changes as an intentional schema break.

### Validation

Add one authoritative boundary:

```cpp
void validate(ProjectState const &project);
```

Run it after deserialization/import and before history installation or audio
publication. Validate at least:

- finite nonempty tuning and positive finite base frequency;
- time signature and maximum duration;
- scale/tuning compatibility and key/transposition bounds;
- finite supported weight, velocity, delay, and gate values;
- recursive model invariants.

Keep predictable user-facing command errors specific. Selection validation belongs to
context resolution, not project validation.

### Explicit targets

Replace callable-signature inference in `increment_state` with explicit cell, element,
or cell-or-element transforms. Serialize `TargetRequirement` in catalog metadata so
the frontend can explain unavailable commands while backend resolution remains
authoritative.

Rename misleading operations such as `delete_cell` when they also delete elements.

## Publication and lifetimes

Publish separate resources:

```text
session.hello -> protocols, catalog, keymap, non-command references
state.get     -> project, ProjectRevision, HistoryEntryId
library.get   -> library, LibraryRevision
state.changed -> project resource only
library.changed -> library resource only
```

Transport/phase remains independent. Audio snapshots contain only render data.

Every changed chord/arp candidate has a history entry. Amendments retain its entry ID
and publish a new project revision. No-op candidates publish no project update.

Advance `LibraryRevision` after every successful replacement/reload, even if values
compare equal, because external files were reread and command-session assumptions must
be invalidated.

### Frontend ingestion

`state.get`, command responses, and `state.changed` must use one `applySnapshot`
operation that:

- rejects lower project revisions;
- accepts equal revisions only as initial/idempotent responses;
- atomically replaces backend-owned project data;
- recomputes derived read models;
- reconciles frontend-owned selection.

Put bridge IO behind a typed client/service. Components call application actions such
as `executeCommand`, `getProject`, and `refreshLibrary`, not the native bridge.

Suggested stores:

- `project`: project snapshot, revision, and entry identity.
- `session`: protocols, catalog, keymap, and references.
- `library`: library snapshot, revision, loading, and errors.
- `ui`: selection, input mode, panels, focus, command bar, scroll, and zoom.
- `transport`: phase and playback animation.

Retain full project snapshots initially. Introduce patches only if profiling shows a
need.

### Command-session and workspace lifetimes

Clear transform and `again` state on project load, host restoration, and reset.
Preserve replay state across undo/redo, informational commands, and failed submissions.
Ordinary measure/tuning imports follow their command policies and are not full project
replacement.

Persist workspace paths as application-wide preferences, not project or host state.
Each processor initializes from those preferences; explicit workspace changes update
them.

## Implementation plan

The implementation work is split into ordered, independently assignable packages in
[`docs/data_model_refactor/`](data_model_refactor/README.md). This document remains the
architecture source of truth; the package files define implementation scope and
completion criteria.

### Phase 1: context and guards

- [ ] Add `ProjectRevision`, `HistoryEntryId`, typed `CommandContext`, and explicit
      command policies.
- [ ] Require expected revisions for project-aware bridge commands.
- [ ] Pass frontend-owned selection and return structural selection suggestions.
- [ ] Move navigation/input mode to the frontend.
- [ ] Add processor tests for stale requests and chord/arp baseline behavior.

### Phase 2: transaction, history, and handlers

- [ ] Add lazy per-resource transaction candidates and stop copying `PluginState`.
- [ ] Return timeline states by const reference.
- [ ] Add narrow capability contexts and transactional file access.
- [ ] Add centralized project validation.
- [ ] Add guarded current-entry amendment and root-history replacement.
- [ ] Move transform/replay state into `CommandSessionState`.
- [ ] Preserve effect rollback and make backend installation non-throwing.
- [ ] Test no-op edits, mixed chains, transform invalidation, undo/redo, save failure,
      and host restoration.

### Phase 3: catalog and publication

- [ ] Finalize catalog metadata and include it in `session.hello`.
- [ ] Implement frontend-local completion/search.
- [ ] Remove backend completion and `catalog.get`.
- [ ] Separate project, library, session, and transport publication.
- [ ] Route frontend project updates through one typed ingestion path.

### Phase 4: domain normalization

- [ ] Add stable scale IDs and active-scale source identity.
- [ ] Remove `scale_shift_index`.
- [ ] Rename/group project and pitch-system state.
- [ ] Apply the intentional project schema break.
- [ ] Persist workspace settings outside project and host state.
