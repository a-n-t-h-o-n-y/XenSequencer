# WebView Bridge Refactor Plan (JUCE 8 + React/TS)

## Goal

Replace the current JUCE widget UI with a JUCE 8 WebView frontend and define a strict, versioned contract between:

- Core/plugin state and commands in C++
- Frontend rendering and interaction in React/TypeScript

Initial strategy: send a full snapshot to the frontend whenever state changes.

---

## Design Constraints

- Single source of truth: C++ core remains authoritative for sequencer/editor state.
- Clean migration: no runtime fallback to legacy JUCE UI path once WebView is adopted.
- Fail fast on environment issues (missing frontend bundle, protocol mismatch).
- Keep v1 simple: command-driven mutations + full-state snapshots.

---

## Bridge Protocol (Fixed Contract)

Use one JSON envelope format for all bridge traffic.

```json
{
  "protocol": "xen.bridge.v1",
  "type": "request | response | event",
  "name": "message_name",
  "request_id": "optional-uuid",
  "payload": {}
}
```

### Frontend -> C++ requests

1. `session.hello`
1. `state.get`
1. `command.execute`
1. `command.completeText`
1. `command.completeId`
1. `catalog.get`
1. `keymap.get`

### C++ -> Frontend events

1. `state.changed` (full snapshot)
   The event is emitted as a JSON-string payload on JUCE event id
   `xenBridgeEvent`.

### Response contract

Each request returns one response with same `request_id`.

### Handshake payloads

`session.hello` request payload:

```json
{
  "protocol": "xen.bridge.v1",
  "snapshot_schema_version": 1,
  "frontend_app": "xen-web-ui",
  "frontend_version": "0.1.0"
}
```

`session.hello` response payload:

```json
{
  "protocol": "xen.bridge.v1",
  "snapshot_schema_version": 1,
  "plugin_version": "v0.3.1"
}
```

Handshake rule:

- Frontend and plugin both require exact match on `protocol` and `snapshot_schema_version`.
- Any mismatch is a hard startup error (no fallback path, no compatibility mode).

### Error response shape

```json
{
  "error": {
    "code": "invalid_request | unsupported_protocol | internal_error",
    "message": "human readable message"
  }
}
```

### UI actions ownership

- UI navigation commands are not bridge commands and not part of C++ command catalog.
- View routing and focus transitions are JS-only actions handled inside frontend state.
- During bridge-first rollout, C++ still accepts legacy UI-navigation command strings.
  Frontend should stop sending them and handle routing locally.

---

## Snapshot Contract

`state.changed` payload and `state.get` response payload share the same schema.

```ts
type MessageLevel = "debug" | "info" | "warning" | "error";
type InputMode = "pitch" | "velocity" | "delay" | "gate" | "scale";
type TranslateDirection = "up" | "down";

type Cell =
  | { type: "Note"; weight: number; pitch: number; velocity: number; delay: number; gate: number }
  | { type: "Rest"; weight: number }
  | { type: "Sequence"; weight: number; cells: Cell[] };

type TimeSignature = { numerator: number; denominator: number };
type Measure = { cell: Cell; time_signature: TimeSignature };
type Tuning = { intervals: number[]; octave: number };
type Scale = { name: string; tuning_length: number; intervals: number[]; mode: number };
type Chord = { name: string; intervals: number[] };

type SelectedState = { measure: number; cell: number[] };

type EngineState = {
  sequence_bank: [
    Measure, Measure, Measure, Measure,
    Measure, Measure, Measure, Measure,
    Measure, Measure, Measure, Measure,
    Measure, Measure, Measure, Measure
  ];
  sequence_names: [
    string, string, string, string,
    string, string, string, string,
    string, string, string, string,
    string, string, string, string
  ];
  tuning: Tuning;
  tuning_name: string;
  scale: Scale | null;
  key: number;
  scale_translate_direction: TranslateDirection;
  base_frequency: number;
};

type EditorState = {
  selected: SelectedState;
  input_mode: InputMode;
};

type LibraryState = {
  scales: Scale[];
  chords: Chord[];
};

type UiStateSnapshot = {
  schema_version: 1;
  snapshot_version: number;
  commit_id: number;
  engine: EngineState;
  editor: EditorState;
  library: LibraryState;
};
```

### Notes

- `snapshot_version` is monotonic and used by frontend to drop stale snapshots.
- `commit_id` follows timeline commit semantics and can stay unchanged for non-engine UI actions.
- `editor.arp_state` is intentionally not exposed in v1 (internal command execution detail).
- `tuning.description` is intentionally omitted in v1 (not used by current UI).

---

## Command Request/Response

### Request: `command.execute`

```json
{
  "command": "set key 11"
}
```

### Response: `command.execute`

```json
{
  "status": {
    "level": "info",
    "message": "Key Set to 11."
  },
  "snapshot": {
    "...": "UiStateSnapshot"
  }
}
```

### Rules

- Always return a full `snapshot` in command response.
- Command status level does not imply no state change.
  - Example: a chain may partially mutate state then fail later.
- Empty/no-op command chains return current snapshot unchanged.
- UI navigation commands are excluded from `command.execute`; frontend handles them locally.

---

## Completion + Catalog Endpoints

### `command.completeText`

Request:

```json
{ "partial": "set se" }
```

Response:

```json
{ "suffix": "quence [String: name] [Int: index=-1]" }
```

### `command.completeId`

Request:

```json
{ "partial": "set se" }
```

Response:

```json
{ "id_suffix": "quence" }
```

### `catalog.get`

Response shape:

```ts
type CatalogArgumentMetadata = {
  type: string;
  name: string;
  default_value: string | null;
};

type CatalogCommandMetadata = {
  path: string[];
  accepts_pattern_prefix: boolean;
  arguments: CatalogArgumentMetadata[];
  description: string;
};

type CatalogGetResponse = {
  commands: CatalogCommandMetadata[];
};
```

Use this to drive frontend command docs, autocomplete UI, and command palette.

---

### `keymap.get`

Returns merged default + user keymap as raw strings.

Response shape:

```ts
type KeymapResponse = {
  keymap: Record<string, Record<string, string>>;
};
```

Notes:

- Values are unparsed command strings (including semicolon chains and `:N=...:`).
- Frontend is responsible for splitting chains and routing UI-only actions locally.

---

## Build Modes

- `Debug` builds load the frontend from `XEN_WEB_UI_DEV_URL` (default:
  `http://127.0.0.1:5173`).
- Non-`Debug` builds require `XEN_WEB_UI_DIST_DIR` and embed all `dist/` assets
  into the plugin binary (`EmbedWebUI`).
- Non-`Debug` configure fails fast if `XEN_WEB_UI_DIST_DIR` is missing, invalid,
  or does not contain `index.html`.
- Current implementation targets single-config generators.
- Editor now uses the WebView bridge UI path only.

## Lifecycle

1. WebView loads frontend.
1. Frontend sends `session.hello` with fixed expected protocol/schema.
1. C++ validates exact protocol/schema match.
1. C++ returns hello response and immediately sends `state.changed` with full snapshot.
1. Frontend requests `catalog.get` once and caches result.
1. User interactions dispatch either:
   - frontend-local UI navigation actions, or
   - `command.execute` for engine/editor state changes.
1. C++ executes command, returns `status + snapshot`.
1. If host/preset changes state outside command execution, C++ emits `state.changed`.

---

## Versioning Strategy

- Envelope protocol: `xen.bridge.v1`
- Snapshot schema: `schema_version: 1`
- Contract is single-version only: plugin and frontend are released together.
- Any breaking contract change increments protocol/schema in lockstep.
- Any mismatch fails fast with a visible startup error.
- No silent downgrade/fallback behavior.

---

## Rollout Plan

### Phase 1: Contract + Bridge Skeleton

- Implement bridge envelope parser/validator in C++.
- Implement `session.hello`, `state.get`, `command.execute`, completion, catalog, and `keymap.get` endpoints.
- Implement snapshot serializer matching `UiStateSnapshot`.
- Add WebView host shell with debug URL loading and release embedded resource loading.

### Phase 2: Frontend State Integration

- Add TS types identical to contract.
- Build a frontend store keyed by `snapshot_version`.
- Render sequence + top bar from snapshot only.

### Phase 3: Interaction Wiring

- Convert engine-edit interactions to command strings.
- Implement frontend-local navigation/focus actions (replacing command-based UI routing).
- Show status message stream from `command.execute` response.
- Wire autocomplete from completion endpoints.

### Phase 4: Parity + Cleanup

- Update default keybinding behavior to call frontend UI actions for view/focus changes.
- Remove remaining docs/examples that present UI routing as executable core commands.
- Remove legacy JUCE UI components and command handler hookups not needed by WebView path.
- Keep one UI path.

---

## Validation Checklist

- Contract test in C++ asserts exact JSON keys/types for `UiStateSnapshot`.
- Contract test verifies snapshot version monotonicity and command response shape.
- Contract test verifies malformed bridge requests return structured `payload.error`.
- Contract test verifies `keymap.get` returns merged raw keymap.
- Frontend runtime validation (zod/io-ts) rejects malformed payloads early.
- Integration test: command -> response snapshot -> rerender loop.

---

## Open Decisions

1. Include transport animation data (`DAWState` + trigger timing) in v1 snapshot or defer to v2 event stream.
1. Keep snake_case JSON keys permanently or map to camelCase at bridge boundary.
1. Timing for final C++ removal of legacy UI-navigation command path at frontend cutover.
