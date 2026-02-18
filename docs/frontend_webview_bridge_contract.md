# Frontend WebView Bridge Contract (Implementation Map)

This file is the implementation-accurate contract for the frontend agent.
It is derived from the current C++ bridge code in:

- `src/webview_bridge.cpp`
- `src/bridge_serialize.cpp`
- `src/gui/webview_host.cpp`

## 1. Transport surface (JUCE WebView)

C++ exposes:

1. Native function name: `xenBridgeRequest`
1. Native event id: `xenBridgeEvent`

`xenBridgeRequest` contract:

- Input: first argument must be a JSON string request envelope
- Output: parsed response envelope object (not a JSON string)

Frontend usage with JUCE helper:

```ts
import { getNativeFunction } from "./juce";

const xenBridgeRequest = getNativeFunction("xenBridgeRequest");
const responseEnvelope = await xenBridgeRequest(JSON.stringify(requestEnvelope));

const removalToken = window.__JUCE__.backend.addEventListener(
  "xenBridgeEvent",
  (eventEnvelope) => {
    // handle state.changed
  }
);

// later:
window.__JUCE__.backend.removeEventListener(removalToken);
```

## 2. Envelope format (fixed)

All requests/responses/events use:

```ts
type Envelope = {
  protocol: "xen.bridge.v1";
  type: "request" | "response" | "event";
  name: string;
  request_id?: string;
  payload: Record<string, unknown>;
};
```

Rules:

1. `protocol` must exactly match `"xen.bridge.v1"`.
1. Requests must have `type: "request"`.
1. `request_id` is optional but, if present, must be a string.
1. Responses echo `request_id` when provided.

## 3. Endpoint map (frontend -> C++)

### `session.hello`

Request payload:

```ts
{
  protocol: "xen.bridge.v1";
  snapshot_schema_version: 1;
  frontend_app: string;
  frontend_version: string;
}
```

Response payload:

```ts
{
  protocol: "xen.bridge.v1";
  snapshot_schema_version: 1;
  plugin_version: string;
}
```

Validation behavior:

1. `protocol` mismatch -> `unsupported_protocol`.
1. `snapshot_schema_version !== 1` -> `unsupported_protocol`.

### `state.get`

Request payload must be an empty object:

```json
{}
```

Response payload is `UiStateSnapshot`.

### `command.execute`

Request payload:

```ts
{
  command: string;
}
```

Response payload:

```ts
{
  status: {
    level: "debug" | "info" | "warning" | "error";
    message: string;
  };
  snapshot: UiStateSnapshot;
}
```

Notes:

1. `status.level` may be `error` while snapshot still reflects partial/previous state.
1. Frontend should not send UI navigation commands through the bridge. Handle those locally.

### `command.completeText`

Request payload:

```ts
{ partial: string }
```

Response payload:

```ts
{ suffix: string }
```

### `command.completeId`

Request payload:

```ts
{ partial: string }
```

Response payload:

```ts
{ id_suffix: string }
```

### `catalog.get`

Request payload must be an empty object.

Response payload:

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

{
  commands: CatalogCommandMetadata[];
}
```

### `keymap.get`

Request payload must be an empty object.

Response payload:

```ts
{
  keymap: Record<string, Record<string, string>>;
}
```

Keymap semantics:

1. Keymap is merged from default + user config on C++ side.
1. Values are raw command strings.
1. Raw strings may contain command chains (`;`) and placeholders like `:N=2:`.
1. Frontend owns parsing/execution policy for UI-local actions.

## 4. Event map (C++ -> frontend)

### `state.changed`

Envelope:

```ts
{
  protocol: "xen.bridge.v1";
  type: "event";
  name: "state.changed";
  payload: UiStateSnapshot;
}
```

Emission behavior:

1. Emitted when `snapshot_version` changes.
1. No `request_id` on events.
1. Do not assume an initial event on startup; call `state.get` after handshake.

## 5. Snapshot payload (`UiStateSnapshot`)

```ts
type MessageLevel = "debug" | "info" | "warning" | "error";
type InputMode = "pitch" | "velocity" | "delay" | "gate" | "scale";
type TranslateDirection = "up" | "down";

type NoteCell = {
  type: "Note";
  weight: number;
  pitch: number;
  velocity: number;
  delay: number;
  gate: number;
};

type RestCell = {
  type: "Rest";
  weight: number;
};

type SequenceCell = {
  type: "Sequence";
  weight: number;
  cells: Cell[];
};

type Cell = NoteCell | RestCell | SequenceCell;

type TimeSignature = {
  numerator: number;
  denominator: number;
};

type Measure = {
  cell: Cell;
  time_signature: TimeSignature;
};

type Tuning = {
  intervals: number[];
  octave: number;
};

type Scale = {
  name: string;
  tuning_length: number;
  intervals: number[];
  mode: number;
};

type Chord = {
  name: string;
  intervals: number[];
};

type UiStateSnapshot = {
  schema_version: 1;
  snapshot_version: number;
  commit_id: number;
  engine: {
    sequence_bank: Measure[]; // currently fixed-size 16 from C++ state
    sequence_names: string[]; // currently fixed-size 16 from C++ state
    tuning: Tuning;
    tuning_name: string;
    scale: Scale | null;
    key: number;
    scale_translate_direction: TranslateDirection;
    base_frequency: number;
  };
  editor: {
    selected: {
      measure: number;
      cell: number[];
    };
    input_mode: InputMode;
  };
  library: {
    scales: Scale[];
    chords: Chord[];
  };
};
```

Not included in v1 snapshot:

1. `editor.arp_state`
1. `tuning.description`
1. DAW transport timing/audio thread fields

## 6. Error payload

On request failure, response `payload` is:

```ts
{
  error: {
    code: "invalid_request" | "unsupported_protocol" | "internal_error";
    message: string;
  };
}
```

Typical triggers:

1. `invalid_request`: wrong/missing field types, unknown request name, non-empty payload where empty is required.
1. `unsupported_protocol`: envelope protocol mismatch, hello protocol/schema mismatch.
1. `internal_error`: uncaught runtime failure.

## 7. Frontend startup sequence (required)

1. Bind `xenBridgeRequest` and subscribe to `xenBridgeEvent`.
1. Send `session.hello`.
1. Validate response protocol/schema.
1. Call `state.get` and set store from returned snapshot.
1. Call `catalog.get` and `keymap.get` and cache both.
1. Start normal command loop with `command.execute` and `state.changed` updates.

## 8. Frontend implementation checklist

1. Add runtime validation (zod/io-ts) for envelopes and every endpoint payload.
1. Treat `state.get` and `state.changed` as the same snapshot schema.
1. Use `snapshot_version` to drop stale state updates.
1. Keep UI navigation as frontend-local actions (do not send through bridge).
1. Preserve raw keymap command strings exactly as delivered.
