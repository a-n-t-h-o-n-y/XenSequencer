# Frontend/Backend Contract and Migration Guide

This is the consolidated handoff for the breaking backend changes that are not yet
implemented in `../xen-frontend`. The current C++ implementation and bridge tests are
authoritative if this document drifts.

There are no compatibility aliases or mixed old/new payloads. The frontend must move
to the contract below as one migration.

## Breaking changes

- Project snapshots use project schema `5` and a grouped `project` object. The old
  `snapshot_version`, `commit_id`, `engine`, and `editor` fields are gone.
- Project history identity and project revision are separate:
  `history_entry_id` identifies the timeline entry, while `project_revision` changes
  whenever the published project changes, including compatible history amendments.
- Selection and input mode are frontend-owned. They are not published in project
  snapshots.
- Targeted commands receive the frontend selection in
  `command.execute.payload.context.selection`.
- Every project-aware command requires
  `command.execute.payload.context.expected_project_revision`.
- Command responses include nullable `suggested_selection`.
- Backend `move ...` and `inputMode ...` commands were removed. Navigation and input
  mode changes must be handled locally.
- Project and library publication are separate revision domains. Library data is not
  included in project snapshots.
- `session.hello` contains the immutable command catalog and revisioned keymap
  resource.
- `catalog.get`, `command.complete`, `command.completeText`, `command.completeId`, and
  the old raw-string keymap contract were removed. Completion and keymap routing are
  frontend-local; keymap persistence uses the dedicated keymap requests.
- Active scales use stable library IDs and preserve an optional `source_id`.
- Composition positions are signed 32-bit coordinates. Rows, columns, and placements
  are sparse; dense `cells` arrays and index-shifting axis commands are gone.

## Transport

JUCE exposes:

- native function: `xenBridgeRequest`;
- native event: `xenBridgeEvent`.

The native function receives one JSON-string argument and resolves to a parsed
response-envelope object.

All messages use:

```ts
type Envelope = {
  protocol: "xen.bridge.v5";
  type: "request" | "response" | "event";
  name: string;
  request_id?: string;
  payload: Record<string, unknown>;
};
```

Requests must use `type: "request"` and an object payload. Responses echo
`request_id` when supplied. Events do not have a request ID.

Errors are returned inside a normal response envelope:

```ts
type ErrorPayload = {
  error: {
    code:
      | "invalid_request"
      | "unsupported_protocol"
      | "internal_error"
      | "conflict"
      | "malformed_document"
      | "keymap_read_error"
      | "keymap_write_error"
      | "keymap_delete_error";
    message: string;
  };
};
```

## Session startup

Request:

```json
{
  "protocol": "xen.bridge.v5",
  "type": "request",
  "name": "session.hello",
  "request_id": "hello-1",
  "payload": {
    "protocol": "xen.bridge.v5",
    "frontend_app": "xen-web-ui",
    "frontend_version": "..."
  }
}
```

Response payload:

```ts
type SessionHello = {
  protocol: "xen.bridge.v5";
  plugin_version: string;
  project_schema_version: 5;
  library_schema_version: 1;
  catalog: {
    schema_version: 3;
    commands: CatalogCommand[];
  };
  keymap: KeymapResource;
};
```

```ts
type KeymapResource = {
  revision: string;
  document: unknown | null;
};
```

Keymap revisions are opaque decimal strings, including
`keymap.write.payload.expected_revision` and
`keymap.delete.payload.expected_revision`. They may exceed both JavaScript's safe
integer range and signed 64-bit range; preserve them as strings and compare only for
equality.

The hello response contains no project or library snapshot. After a successful hello,
request both `state.get` and `library.get`. Do not wait for initial change events; the
backend only emits them after a revision changes.

There is no combined `snapshot_schema_version` handshake field. Validate the separate
project, library, and catalog schema versions returned by the backend.

## Command catalog and completion

```ts
type CatalogConstraint = {
  kind: string;
  minimum: number | null;
  maximum: number | null;
  values: string[];
};

type CatalogArgument = {
  kind: string;
  display_name: string;
  required: boolean;
  default_value: string | null;
  constraints: CatalogConstraint[];
};

type CatalogCommand = {
  path: string[];
  keywords: string[];
  accepts_pattern_prefix: boolean;
  target_requirement: "none" | "cell" | "element" | "cell_or_element";
  arguments: CatalogArgument[];
  description: string;
};
```

Cache `session.hello.payload.catalog.commands` and use it for command help,
autocomplete, filtering, and ranking. Completion should tolerantly parse only the
active semicolon-delimited chain segment. Final command text is still submitted to the
strict backend parser.

The opaque keymap resource, whole-document requests, revision rules, and frontend
ownership requirements are defined in
[`frontend_keymap_contract.md`](frontend_keymap_contract.md).

## Project resource

`state.get`, `state.changed`, and `command.execute.payload.snapshot` all use the same
payload:

```ts
type Note = {
  type: "Note";
  pitch: number;
  velocity: number;
  delay: number;
  gate: number;
};

type Sequence = {
  type: "Sequence";
  cells: Cell[];
};

type MusicElement = Note | Sequence;

type Cell = {
  weight: number;
  elements: MusicElement[];
};

type ScaleDefinition = {
  name: string;
  tuning_length: number;
  intervals: number[];
  mode: number;
};

type PitchState = {
  tuning: {
    name: string;
    definition: {
      intervals: number[];
      octave: number;
    };
  };
  scale: {
    source_id: string | null;
    definition: ScaleDefinition;
  } | null;
  transposition: number;
  translation_direction: "up" | "down";
  base_frequency: number;
};

type CompositionColumn = {
  duration: {
    numerator: number;
    denominator: number;
  };
  pitch: PitchState;
};

type ProjectSnapshot = {
  schema_version: 5;
  history_entry_id: number;
  project_revision: number;
  project: {
    sequence_bank: {
      next_id: number;
      sequences: Array<{
        id: number;
        name?: string;
        cell: Cell;
      }>;
    };
    composition: {
      default_column: CompositionColumn;
      columns: Array<CompositionColumn & {
        coordinate: number;
      }>;
      rows: Array<{
        coordinate: number;
        name?: string;
        channel_id: string;
      }>;
      placements: Array<{
        row: number;
        column: number;
        sequence_id: number;
      }>;
      loop_region: {
        start_column: number;
        end_column: number;
      };
    };
  };
};
```

An empty `Cell.elements` array represents silence. Musical content lives in
`sequence_bank.sequences[].cell`; arrangement lives in `composition.placements`.
An absent placement is an empty/rest cell and consumes no serialized storage.
Materialized rows and columns always have at least one placement. Missing horizontal
coordinates inside the loop are silent measures using `default_column.duration`.
Materialized columns carry their own duration and pitch state.

Coordinates must be integers in `[-2147483648, 2147483647]`. `(0, 0)` is the default
placement and remains stable when negative coordinates are added. Loop bounds may
reference implicit columns, must satisfy `start_column <= end_column`, and do not
automatically expand when content is created outside them. Playback rebases the loop
start to local sample zero.

The active scale embeds the complete musical definition. `source_id` identifies the
library entry used to create it and may be null for an embedded/untracked scale.
Chromatic state is represented by `scale: null`.

### Project ingestion

Use one ingestion function for `state.get`, `state.changed`, and command response
snapshots:

1. Reject schemas other than `5`.
2. Install the first valid snapshot.
3. Ignore an older `project_revision`.
4. Treat an equal revision as an idempotent duplicate.
5. Install a newer revision, then reconcile frontend-owned selection against the new
   sequence. Fall back to the root selection when the path no longer resolves.

Do not use `history_entry_id` for freshness. It can stay unchanged while
`project_revision` advances.

## Frontend-owned selection

```ts
type Selection = {
  path: Array<
    | { kind: "element"; index: number }
    | { kind: "cell"; index: number }
  >;
};
```

The root cell is `{ path: [] }`. Paths alternate:

1. `element` indexes `Cell.elements`;
2. `cell` indexes the child cells of the selected `Sequence`;
3. repeat as needed.

A path ending in `element` selects a `MusicElement`. An empty path or a path ending in
`cell` selects a `Cell`.

The frontend owns selection traversal, input mode, focus, panels, command text, zoom,
and scroll state. Backend snapshots must not overwrite them.

## Command execution

Request:

```ts
type CommandExecuteRequest = {
  command: string;
  context?: {
    expected_project_revision?: number;
    selection?: Selection;
    cursor: {
      row_coordinate: number;
      column_coordinate: number;
      sequence_id: number | null;
    };
  };
};
```

Project-aware commands fail when `expected_project_revision` is missing or stale.
Targeted commands fail when selection is missing, invalid, or resolves to the wrong
target kind.

The catalog exposes target requirements but not project-awareness. The simplest safe
frontend policy is to include the current `project_revision` and current valid
selection with every command. The backend ignores selection for untargeted commands
and does not require a revision for commands that do not access project state.

Response payload:

```ts
type CommandExecuteResponse = {
  status: {
    level: "debug" | "info" | "warning" | "error";
    message: string;
  };
  suggested_selection: Selection | null;
  snapshot: ProjectSnapshot;
};
```

Always ingest `snapshot` through the normal project-ingestion path. If
`suggested_selection` is non-null, adopt it after installing the snapshot. Otherwise
keep the existing selection if it still resolves, falling back to the root when it
does not.

Current structural suggestions include:

- `duplicate`: select the duplicate;
- `delete` and `cut`: select the nearest surviving sibling or parent;
- pasting a cell over an element: select the parent cell;
- successful non-structural targeted edits: retain the submitted selection.

`undo` and `redo` are still backend commands, must be submitted alone, and require the
current project revision.

Composition commands use signed coordinates:

```text
composition cell assign <row> <column> <sequence-name>
composition cell clear <row> <column>
composition cell move <from-row> <from-column> <to-row> <to-column>
composition row rename <row> <name>
composition row channel <row> <channel-id>
composition loop start <column>
composition loop end <column>
```

Assigning at an empty position atomically materializes missing axes. A new row inherits
the nearest materialized row's channel and a new column copies the nearest materialized
column; ties prefer the candidate nearest coordinate zero, then the lower coordinate.
Clearing the last placement on an axis removes that axis metadata. Moving preserves the
sequence reference, materializes destination axes before pruning source axes, and fails
if the source is absent or destination occupied. Row/column insert, delete, move, and
duplicate commands no longer exist.

## Library resource

`library.get` and `library.changed` share:

```ts
type LibrarySnapshot = {
  schema_version: 1;
  library_revision: number;
  paths: {
    library: string;
    content: string;
    tunings: string;
  };
  cells: Array<{
    name: string;
    relative_path: string;
    stem: string;
    path: string;
    command: string;
  }>;
  compositions: Array<{
    name: string;
    relative_path: string;
    stem: string;
    path: string;
    command: string;
  }>;
  tunings: Array<{
    name: string;
    relative_path: string;
    stem: string;
    path: string;
    command: string;
    description: string;
    intervals: number[];
    octave: number;
    note_count: number;
  }>;
  scales: Array<
    | {
        id: "chromatic";
        name: "chromatic";
        definition: null;
        intervals: [];
        command: string;
      }
    | {
        id: string;
        definition: ScaleDefinition;
        command: string;
      }
  >;
  chords: Array<{
    name: string;
    intervals: number[];
    command: string;
  }>;
  commands: {
    reload_scales: "load scales";
    reload_chords: "load chords";
    library_directory: "libraryDirectory";
  };
};
```

Project and library revisions are independent. Use a separate revision-aware ingestion
path for library responses/events.

`library.get` scans sequence and tuning files recursively on each request. Scales and
chords reflect backend memory; execute `load scales` or `load chords` to reload those
files. Library reloads advance `library_revision` even when the loaded values compare
equal. Workspace path changes also publish a new library revision.

The library payload no longer contains active tuning/scale fields. Read active pitch
state from the project snapshot.

The frontend presents cell documents as reusable sequences and aggregates them with
composition documents, tunings, and scales in Quick Access. Sequence and composition
documents appear in its Files scope; chords remain cached as command-argument data
and are not standalone Quick Access actions. This is presentation behavior and does
not add a bridge endpoint.

## Events

All events arrive through `xenBridgeEvent`.

```ts
type BridgeEvent =
  | Envelope & { name: "state.changed"; payload: ProjectSnapshot }
  | Envelope & { name: "library.changed"; payload: LibrarySnapshot }
  | Envelope & {
      name: "transport.phase.sync";
      payload: { bpm: number; phase: number };
    }
  | Envelope & { name: "transport.stopped"; payload: {} }
  | Envelope & { name: "keymap.changed"; payload: KeymapResource };
```

`transport.phase.sync.phase` is normalized to `[0, 1)`. Treat transport events as
transient animation state; they do not participate in project or library revisions.
`transport.stopped` is the authoritative stop edge.

## Required frontend migration

The frontend migration must cover the resource contracts below as one breaking
change. Keymap-specific replacement work is tracked separately in
[`frontend_keymap_contract.md`](frontend_keymap_contract.md).

At minimum:

- remove the old `snapshot_schema_version` hello field and validate the three returned
  resource/catalog schema versions;
- replace project schema `4` parsing with project schema `5`;
- replace `snapshot_version`/`commit_id` with
  `project_revision`/`history_entry_id`;
- replace flat `engine` fields with `project.sequence_bank` and
  `project.composition`, including each column's `pitch` context;
- replace dense row/column indices and `cells` arrays with signed coordinates,
  materialized axis records, and sparse placements;
- render a finite viewport of virtual empty cells instead of iterating the stored
  row/column Cartesian product;
- remove row/column insert/delete controls and make selection navigation unbounded;
- remove all reads of `snapshot.editor`;
- keep selection and input mode in frontend state;
- implement local selection navigation and input-mode actions;
- send current revision and selection in command context;
- consume `suggested_selection`;
- consume the catalog and opaque keymap resource from `session.hello`;
- own keymap defaults, validation, merging, trigger matching, and target dispatch;
- implement whole-document keymap responses and `keymap.changed`;
- remove all `command.complete*` and `catalog.get` requests;
- implement completion from the cached catalog;
- add `library.changed` handling and revision-aware library ingestion;
- remove assumptions that library payloads contain active tuning/scale state;
- use stable scale IDs and backend-provided command strings;
- update runtime validators and tests for all new payloads.

Relevant backend anchors:

- `src/webview_bridge.cpp`
- `src/bridge_serialize.cpp`
- `src/gui/webview_host.cpp`
- `src/xen_processor.cpp`
- `test/core/webview_bridge.test.cpp`
- `test/processor/processor_commands.test.cpp`
- `test/data_model_refactor.test.cpp`
