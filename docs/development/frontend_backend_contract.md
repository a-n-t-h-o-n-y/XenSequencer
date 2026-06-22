# Frontend/Backend Contract and Migration Guide

This is the consolidated handoff for the breaking backend changes that are not yet
implemented in `../xen-frontend`. The current C++ implementation and bridge tests are
authoritative if this document drifts.

There are no compatibility aliases or mixed old/new payloads. The frontend must move
to the contract below as one migration.

## Breaking changes

- Project snapshots use project schema `1` and a grouped `project` object. The old
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
- `session.hello` contains the immutable command catalog and merged keymap.
- `catalog.get`, `command.complete`, `command.completeText`, `command.completeId`, and
  `keymap.get` were removed. Completion and keymap routing are frontend-local.
- Active scales use stable library IDs and preserve an optional `source_id`.

## Transport

JUCE exposes:

- native function: `xenBridgeRequest`;
- native event: `xenBridgeEvent`.

The native function receives one JSON-string argument and resolves to a parsed
response-envelope object.

All messages use:

```ts
type Envelope = {
  protocol: "xen.bridge.v1";
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
    code: "invalid_request" | "unsupported_protocol" | "internal_error";
    message: string;
  };
};
```

## Session startup

Request:

```json
{
  "protocol": "xen.bridge.v1",
  "type": "request",
  "name": "session.hello",
  "request_id": "hello-1",
  "payload": {
    "protocol": "xen.bridge.v1",
    "frontend_app": "xen-web-ui",
    "frontend_version": "..."
  }
}
```

Response payload:

```ts
type SessionHello = {
  protocol: "xen.bridge.v1";
  plugin_version: string;
  project_schema_version: 1;
  library_schema_version: 1;
  catalog: {
    schema_version: 1;
    commands: CatalogCommand[];
  };
  keymap: Record<string, Record<string, string>>;
};
```

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

The keymap contains raw strings. Values may contain command chains, placeholders such
as `:N=2:`, and frontend-local actions. The frontend must split and route those
locally; removed navigation/input-mode commands must not be sent to the backend.

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

type ProjectSnapshot = {
  schema_version: 1;
  history_entry_id: number;
  project_revision: number;
  project: {
    measure: {
      cell: Cell;
      time_signature: {
        numerator: number;
        denominator: number;
      };
    };
    pitch: {
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
  };
};
```

An empty `Cell.elements` array represents silence. The project has one top-level
measure; the old sequence bank is gone.

The active scale embeds the complete musical definition. `source_id` identifies the
library entry used to create it and may be null for an embedded/untracked scale.
Chromatic state is represented by `scale: null`.

### Project ingestion

Use one ingestion function for `state.get`, `state.changed`, and command response
snapshots:

1. Reject schemas other than `1`.
2. Install the first valid snapshot.
3. Ignore an older `project_revision`.
4. Treat an equal revision as an idempotent duplicate.
5. Install a newer revision, then reconcile frontend-owned selection against the new
   measure. Fall back to the root selection when the path no longer resolves.

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

## Library resource

`library.get` and `library.changed` share:

```ts
type LibrarySnapshot = {
  schema_version: 1;
  library_revision: number;
  paths: {
    library: string;
    sequences: string;
    tunings: string;
  };
  measures: Array<{
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

`library.get` scans measure and tuning files recursively on each request. Scales and
chords reflect backend memory; execute `load scales` or `load chords` to reload those
files. Library reloads advance `library_revision` even when the loaded values compare
equal. Workspace path changes also publish a new library revision.

The library payload no longer contains active tuning/scale fields. Read active pitch
state from the project snapshot.

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
  | Envelope & { name: "transport.stopped"; payload: {} };
```

`transport.phase.sync.phase` is normalized to `[0, 1)`. Treat transport events as
transient animation state; they do not participate in project or library revisions.
`transport.stopped` is the authoritative stop edge.

## Required frontend migration

The current sibling frontend still uses the old contract. In particular, it requests
snapshot schema `4`, parses `engine` and `editor`, calls `keymap.get` and
`command.completeText`, ignores `library.changed`, and submits backend commands
without revision or selection context.

At minimum:

- remove the old `snapshot_schema_version` hello field and validate the three returned
  resource/catalog schema versions;
- replace snapshot schema `4` parsing with project schema `1`;
- replace `snapshot_version`/`commit_id` with
  `project_revision`/`history_entry_id`;
- replace flat `engine` fields with `project.measure` and `project.pitch`;
- remove all reads of `snapshot.editor`;
- keep selection and input mode in frontend state;
- implement local selection navigation and input-mode actions;
- send current revision and selection in command context;
- consume `suggested_selection`;
- consume catalog and keymap from `session.hello`;
- remove the `keymap.get` startup request;
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
