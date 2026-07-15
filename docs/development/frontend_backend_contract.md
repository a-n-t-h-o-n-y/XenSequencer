# Frontend/Backend Contract

This document is the authoritative handoff for the native backend. The persistence
cutover is intentionally breaking: there are no compatibility aliases or mixed old
and new payloads.

## Transport and schemas

JUCE exposes the native function `xenBridgeRequest` and event `xenBridgeEvent`.

```ts
type Envelope = {
  protocol: "xen.bridge.v9";
  type: "request" | "response" | "event";
  name: string;
  request_id?: string;
  payload: Record<string, unknown>;
};
```

`session.hello` reports:

```ts
type CatalogCommand = {
  path: string[];
  keywords: string[];
  accepts_pattern_prefix: boolean;
  target_requirement: "none" | "cell" | "element" | "cell_or_element";
  arguments: Array<{
    kind: string;
    display_name: string;
    required: boolean;
    default_value: string | null;
    constraints: Array<{
      kind: string;
      minimum: number | null;
      maximum: number | null;
      values: string[];
    }>;
  }>;
  description: string;
};

type ModulationDestinationCatalogEntry = {
  id: "pitch" | "velocity" | "delay" | "gate" | "weight" | "midi_cc";
  range: "integer" | "unit" | "positive";
  quantization?: "nearest";
  parameters: Array<{
    id: "controller";
    kind: "integer";
    required: true;
    constraints: [{ kind: "range"; minimum: 0; maximum: 127 }];
  }>;
};

type ModulationCatalog = {
  schema_version: 3;
  maximum_waveforms: 64;
  waveform_shapes: Array<
    "sine" | "triangle" | "sawtooth_up" | "sawtooth_down" | "square"
  >;
  waveform_parameters: {
    frequency: { minimum: 0; maximum: 64 };
    phase: { minimum: 0; maximum: 1 };
    amplitude: { minimum: -1; maximum: 1 };
    amplitude_offset: { minimum: -1; maximum: 1 };
  };
  operations: Array<{
    id: "average" | "sum" | "product" | "am" | "ring" | "fm" | "pm";
    minimum_enabled_waveforms?: 1;
    enabled_waveforms?: 2;
    roles?: ["carrier", "modulator"];
  }>;
  destinations: ModulationDestinationCatalogEntry[];
  normalization: "clamp((raw + 1) / 2, 0, 1)";
};

type SessionHello = {
  protocol: "xen.bridge.v9";
  plugin_version: string;
  project_schema_version: 7;
  library_schema_version: 2;
  catalog: { schema_version: 7; commands: CatalogCommand[] };
  modulation: ModulationCatalog;
  binding: { session_id: string; instance_id: string; channel_id: string };
  keymap: KeymapResource;
  preferences: PreferencesResource;
};
```

All history, state, project, library, keymap, and preferences revisions crossing the
WebView boundary are decimal strings. Preserve them losslessly and parse numeric
revisions as `BigInt`. Order project snapshots only by `state_revision`;
`project_revision` is an optimistic-edit token and can jump when persisted history is
restored. File and recovery revisions are opaque strings compared only for equality.

Errors use normal response envelopes:

```ts
type ErrorPayload = {
  error: {
    code:
      | "invalid_request"
      | "unsupported_protocol"
      | "internal_error"
      | "stale_project"
      | "unsaved_changes"
      | "invalid_path"
      | "not_found"
      | "file_exists"
      | "file_conflict"
      | "project_path_required"
      | "file_too_large"
      | "invalid_document"
      | "preview_active"
      | "recovery_conflict"
      | "io_error"
      | string;
    message: string;
    details?: { current_file_revision?: string | null };
  };
};
```

## Project state and document lifecycle

```ts
type FileRevision = string; // opaque "sha256:<hex>" token

type Selection = {
  path: Array<
    | { kind: "element"; index: number }
    | { kind: "cell"; index: number }
  >;
};

type Cursor = {
  row_coordinate: number;
  column_coordinate: number;
  sequence_id: number | null;
};

type MidiCcEntry = {
  controller: number; // unique integer in [0, 127]
  value: number; // finite normalized value in [0, 1]
};

type MidiCcLabel = {
  controller: number; // unique integer in [0, 127]
  label: string; // 1–4096 UTF-8 bytes
};

type Note = {
  type: "Note";
  pitch: number;
  velocity: number;
  delay: number;
  gate: number;
  midi_cc: MidiCcEntry[];
};

type Project = {
  sequence_bank: SequenceBank;
  composition: Composition;
  midi_cc_labels: MidiCcLabel[];
};

type ProjectSnapshot = {
  schema_version: 7;
  state_revision: string;
  project_revision: string;
  history_entry_id: string;
  preview_active: boolean;
  document: {
    relative_path: string | null;
    display_name: string;
    dirty: boolean;
    file_revision: FileRevision | null;
  };
  recovery: null | {
    revision: string;
    saved_at_unix_ms: string;
    relative_path: string | null;
    project_revision: string;
  };
  project: Project;
};

type DocumentFile = {
  name: string;
  relative_path: string;
  stem: string; // content-relative path without the extension
  file_revision: FileRevision;
};

type DocumentOperationResult = {
  snapshot: ProjectSnapshot;
  file: DocumentFile | null;
  suggested_selection: Selection | null;
};
```

`Note.midi_cc` and `Project.midi_cc_labels` are always present and sorted by numeric
controller number. A zero-valued entry remains present; only a missing entry means the
controller is absent. Controller labels are unique under ASCII case-insensitive
comparison and otherwise retain their exact spelling. The frontend owns label lookup
and display behavior.

`state.get`, `state.changed`, document responses, generic preview responses,
modulation begin/end responses, and `command.execute.payload.snapshot` all carry this
shape. Ingest snapshots by
`state_revision`; use `project_revision` for optimistic project edits. A document save
can advance `state_revision` without changing `project_revision`.

The backend owns current path, clean baseline, dirty state, file-conflict token,
recovery state, and unsaved-change enforcement for the shared multi-instance session.
The frontend owns confirmation dialogs and sends an explicit confirmation on retry.

### Project requests

```ts
type ProjectNewRequest = {
  expected_project_revision: string;
  discard_unsaved: boolean;
};

type ProjectOpenRequest = ProjectNewRequest & {
  relative_path: string; // exact content-relative .xenproj path
};

type ProjectSaveRequest = {
  expected_project_revision: string;
};

type ProjectSaveAsRequest = ProjectSaveRequest & {
  relative_path: string;
  expected_file_revision: FileRevision | null;
};

type RecoveryRestoreRequest = ProjectNewRequest & {
  recovery_revision: string;
};

type RecoveryDiscardRequest = { recovery_revision: string };
```

Request names are `project.new`, `project.open`, `project.save`, `project.save_as`,
`project.recovery.restore`, and `project.recovery.discard`. New/open with
`discard_unsaved: false` also protect a pending recovery. Recovery restore requires
discard confirmation only when the current document itself is dirty.
Every successful response payload is a `DocumentOperationResult`; project saves and
opens populate `file`.

For Save As, `expected_file_revision: null` is create-only. If the target exists, the
backend returns `file_exists` and the current token in error details. After confirmation,
retry with that token. A later mismatch returns `file_conflict`. `project.save` uses the
backend-owned current path and token; an untitled project returns
`project_path_required`. To confirm an external change reported by `project.save`, retry
as `project.save_as` with the same `relative_path` and the returned current token (or
`null` when the file was deleted).

### Cell requests

```ts
type CellImportRequest = {
  relative_path: string; // exact content-relative .xencell path
  expected_project_revision: string;
  cursor: Cursor;
};

type CellSaveRequest = CellImportRequest & {
  selection: Selection; // must resolve to a Cell
  expected_file_revision: FileRevision | null;
};
```

Request names are `cell.import` and `cell.save`. Import creates and arranges a new
sequence named from the file stem. Save exports the selected recursive cell. Both use
`DocumentOperationResult`; import populates `file` and `suggested_selection`, while
save populates `file` and echoes the resolved selection. Cell saves use the same
create-only/CAS overwrite protocol as Project Save As: send `null` first, then retry
with the backend's current file token only after user confirmation.
Cell files are reusable assets and never become the current project document.

All document paths are nested relative paths under the configured content directory.
Absolute paths, traversal, symlink escapes, non-portable names, and wrong extensions
are rejected by the backend.

## Commands and previews

`command.execute` remains the general editing API:

```ts
type CommandExecuteRequest = {
  command: string;
  context?: {
    expected_project_revision?: string;
    selection?: Selection;
    preview_id?: string;
    cursor: Cursor;
  };
};
```

The response contains status, nullable `suggested_selection`, and the current
`snapshot`. Project-aware commands require a current project revision. The command
catalog remains immutable per hello response and drives frontend completion.

Per-note MIDI CC editing and persistent labels use these catalog-advertised commands:

```text
set midiCC <controller:0..127> <value:0..1>
shift midiCC <controller:0..127> <amount:-1..1>
remove midiCC <controller:0..127>
set midiCCLabel <controller:0..127> <label>
remove midiCCLabel <controller:0..127>
```

Their catalog argument kinds and constraints are exact: `midi_controller` is a
required integer with a `[0,127]` range constraint; `normalized_value` is a required
float with `[0,1]`; `normalized_delta` is a required float with `[-1,1]`; and the
label is a required `string` with a `byte_length` constraint of `[1,4096]`.

The first three accept the existing pattern prefix and require a cell-or-element
selection. Set materializes or overwrites the selected controller. Shift uses `0.5`
for each note where the controller is absent, then clamps to `[0,1]`. Remove is a
successful no-op where absent. Label commands do not accept a pattern or selection;
labels containing whitespace use the existing quoted-token syntax. All five commands
participate in revisions, committed history, dirty state, recovery, undo/redo, and
multi-instance snapshot publication. Successful no-ops do not advance project history.

Document commands are retained for the keyboard command line and use the same backend
service:

```text
project new
project open <relative-path.xenproj>
project save
project save as <relative-path.xenproj>
load cell <relative-path.xencell>
save cell <relative-path.xencell>
load tuning <relative-path.scl>
```

Commands never discard dirty work or overwrite an existing file. When confirmation is
needed, the frontend retries through the structured API.

Preview requests are `preview.begin`, `preview.commit`, and `preview.cancel`; every
`expected_project_revision` is a decimal string. Document operations are rejected while
a preview is active. Processor/DAW persistence always saves the persistent baseline,
not transient preview state.

Modulation uses the separate `modulation.preview.begin`, `.update`, `.commit`, and
`.cancel` lifecycle. Destinations are parameterized objects in schema 3:

```ts
type ModulationDestination =
  | { id: "pitch" }
  | { id: "velocity" }
  | { id: "delay" }
  | { id: "gate" }
  | { id: "weight" }
  | { id: "midi_cc"; controller: number };

type ModulationPreviewUpdateRequest = {
  preview_id: string;
  update_sequence: string;
  expected_project_revision: string;
  destination: ModulationDestination;
  output_range: { minimum: number; maximum: number };
  modulation: {
    operation: "average" | "sum" | "product" | "am" | "ring" | "fm" | "pm";
    waveforms: Array<{
      enabled: boolean;
      shape: "sine" | "triangle" | "sawtooth_up" | "sawtooth_down" | "square";
      frequency: number;
      phase: number;
      amplitude: number;
      amplitude_offset: number;
    }>;
  };
};
```

The `midi_cc` controller is a required integer in `[0,127]`, and its output-range
endpoints must be finite and contained in `[0,1]`. Applying it writes ordinary
`Note.midi_cc` entries, including explicit zero values and controllers that were absent
in the preview baseline. Each update replaces the staged result from that baseline.
Begin/commit/cancel responses contain snapshots; update responses are small
acknowledgements and intentionally omit the snapshot. Accepted updates publish
coalesced `state.changed` events at the coordinator maintenance rate. Commit creates
one undoable edit and cancel restores the baseline.

### MIDI CC rendering

The backend quantizes controller values as `round(value * 127)`, so `0.5` becomes
`64`. Each CC uses the same MPE member channel and start sample as its note. Generated
events at a shared beat are deterministic: note-off boundaries precede starts;
simultaneous starts use logical-note-key order; and each start emits any voice-steal
note-off, pitch bend, controllers in ascending numeric order, then note-on. Attached
controllers are emitted even when note velocity is zero. Zero-duration notes remain
suppressed. Schedule adoption and transport reconciliation re-emit the currently
attached values for held notes; removing a controller does not synthesize a reset.

## Library resource

```ts
type ContentFile = {
  name: string;
  relative_path: string;
  stem: string; // content-relative path without the extension
  file_revision: FileRevision;
  command: string;
};

type LibrarySnapshot = {
  schema_version: 2;
  library_revision: string;
  paths: { library: string; content: string; tunings: string };
  cells: ContentFile[];
  projects: ContentFile[];
  tunings: Array<ContentFile & {
    description: string;
    intervals: number[];
    octave: number;
    note_count: number;
  }>;
  scales: unknown[];
  chords: unknown[];
  commands: Record<string, string>;
};
```

`library.get` and `library.changed` use this schema. Project discovery recognizes only
`.xenproj`; the old `compositions` key and absolute per-entry `path` fields are gone.
Successful project saves and cell exports advance `library_revision` and publish a
library event to every instance.

## Persistence formats and limits

- `.xencell`: reusable cell, `{ "kind": "xen_cell", "schema": 2, ... }`, 16 MiB.
- `.xenproj`: complete project, `{ "kind": "xen_project", "schema": 2, ... }`,
  64 MiB.
- DAW state: internal `{ "kind": "xen_processor_state", "schema": 6, ... }`,
  65 MiB including its envelope.
- Recovery: internal `{ "kind": "xen_recovery", "schema": 2, ... }`, 65 MiB
  including its envelope.

`.xencomp` and `xen_composition` are unsupported. Project files embed their complete
tuning and active-scale definitions. JSON structural depth and event counts are bounded;
cell recursion above 64, cell assets above 100,000 musical nodes, and projects above
250,000 aggregate nodes are rejected.

Dirty persistent projects are atomically autosaved within two seconds after recovery
work becomes pending, even during continuous editing.
The active run's autosave is internal and does not populate `snapshot.recovery`. After
restart, that field is populated only when the autosave is newer than and differs from
the restored DAW state. If no DAW state is supplied, any valid recovery for the session
is offered. A pending candidate blocks edits, previews, binding changes, project saves,
imports, and forced autosave until the frontend explicitly restores or discards it.
Manual project save, opening/new-project replacement, or undoing exactly to the saved
content clears the active recovery file.

## Events

The native `xenBridgeEvent` emits:

```ts
type BridgeEvent =
  | (Envelope & { name: "state.changed"; payload: ProjectSnapshot })
  | (Envelope & { name: "library.changed"; payload: LibrarySnapshot })
  | (Envelope & { name: "keymap.changed"; payload: KeymapResource })
  | (Envelope & { name: "preferences.changed"; payload: PreferencesResource })
  | (Envelope & { name: "phase.sync" | "transport.stopped"; payload: object });
```

After `session.hello`, request both `state.get` and `library.get`; initial events are
not guaranteed. Always ingest snapshots returned directly by a request as well as
events, treating equal revisions as idempotent duplicates.
