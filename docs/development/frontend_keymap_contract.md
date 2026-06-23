# Frontend Keymap Contract

This document defines the backend keymap resource and the frontend changes required
to consume and edit it. The old YAML keymap and command-like frontend actions have
been removed. There is no compatibility path.

## Ownership

The frontend owns:

- browser keyboard events;
- key matching and active UI context selection;
- selection movement, input mode, focus, panels, modals, and other UI-only state;
- the settings UI used to capture and edit shortcuts.

The backend owns:

- compiled default bindings;
- validation of persisted targets;
- user override persistence;
- optimistic concurrency and keymap revisioning;
- publication of the effective keymap.

The frontend must never write the keymap file directly. The backend stores machine
generated JSON at `XenSequencer/settings/keymap.json` under the OS application-data
directory. The file contains only user overrides. Defaults are compiled into the
application.

## Keyboard Identity

Triggers use `KeyboardEvent.key`, not `KeyboardEvent.code`. The settings UI should
capture `event.key` and the modifier state from the user's keypress.

The `command` modifier is logical:

- use `event.metaKey` on macOS;
- use `event.ctrlKey` on Windows and Linux.

Normalize a single ASCII letter (`A` through `Z`) to lowercase before matching or
submitting it. Preserve all other values exactly. This keeps the character identity
stable while representing Shift separately; for example, Shift+H is
`{ key: "h", modifiers: { shift: true, ... } }`. Values such as `ArrowLeft`,
`Escape`, `Delete`, `+`, and non-ASCII printable characters are otherwise matched
exactly. The frontend may use a display formatter, but must preserve the normalized
protocol value.

## Keymap Resource

`session.hello.payload.keymap`, `keymap.get`, keymap mutation responses, and the
`keymap.changed` event all contain the same resource:

```ts
type KeymapTrigger = {
  key: string
  modifiers: {
    shift: boolean
    command: boolean
    alt: boolean
  }
  when?: {
    input_mode: 'pitch' | 'velocity' | 'delay' | 'gate' | 'scale'
  }
}

type CommandTarget = {
  type: 'command'
  command: string
}

type UiActionTarget =
  | {
      type: 'ui_action'
      action: 'selection.move'
      arguments: {
        direction: 'left' | 'right' | 'up' | 'down'
        amount: number
      }
    }
  | {
      type: 'ui_action'
      action: 'input_mode.set'
      arguments: {
        mode: 'pitch' | 'velocity' | 'delay' | 'gate' | 'scale'
      }
    }

type KeymapTarget = CommandTarget | UiActionTarget

type KeymapBinding = {
  trigger: KeymapTrigger
  target: KeymapTarget
}

type KeymapOverride = {
  context: string
  trigger: KeymapTrigger
  target: KeymapTarget | null
}

type KeymapResource = {
  schema_version: 1
  revision: number
  key_semantics: 'KeyboardEvent.key'
  bindings: Record<string, KeymapBinding[]>
  overrides: KeymapOverride[]
}
```

`bindings` is the effective result after applying user overrides to defaults.
`overrides` is included so the settings UI can distinguish:

- an inherited default;
- a user replacement;
- a user-added binding;
- an explicit unbinding, represented by `target: null`.

The initial context is `sequence`. Context selection and precedence are frontend
concerns. Additional contexts can be introduced without changing the resource shape.

## Runtime Dispatch

For each keyboard event:

1. Ignore application shortcuts when the event target is an editable text control,
   unless the active UI explicitly captures that key.
2. Select the active context.
3. Match `key`, all three modifiers, and the optional `when.input_mode`.
4. Call `preventDefault()` only after a binding matches.
5. Expand numeric command placeholders such as `:N=2:` for command targets.
6. Dispatch by target type.

For `type: "command"`, submit the resulting string through `command.execute` with
the current project revision and reconciled selection.

For `type: "ui_action"`, dispatch through a closed frontend action registry. Do not
send UI actions to the backend command parser. Unknown action IDs or invalid
arguments are contract errors and should be surfaced during ingestion.

The old semicolon-based mixed local/backend routing is obsolete. Each binding has one
typed target.

## Requests

### `keymap.get`

Request:

```json
{
  "protocol": "xen.bridge.v1",
  "type": "request",
  "name": "keymap.get",
  "request_id": "keymap-1",
  "payload": {}
}
```

The response payload is `KeymapResource`.

### `keymap.override.set`

Creates or replaces the override identified by `(context, trigger)`.

```json
{
  "expected_revision": 4,
  "context": "sequence",
  "trigger": {
    "key": "q",
    "modifiers": {
      "shift": false,
      "command": false,
      "alt": false
    }
  },
  "target": {
    "type": "command",
    "command": "rest"
  }
}
```

Set `target` to `null` to explicitly disable a default binding:

```json
{
  "expected_revision": 5,
  "context": "sequence",
  "trigger": {
    "key": "ArrowLeft",
    "modifiers": {
      "shift": false,
      "command": false,
      "alt": false
    }
  },
  "target": null
}
```

The response is the complete updated `KeymapResource`.

### `keymap.override.remove`

Removes an override and reveals the compiled default, if one exists:

```json
{
  "expected_revision": 6,
  "context": "sequence",
  "trigger": {
    "key": "ArrowLeft",
    "modifiers": {
      "shift": false,
      "command": false,
      "alt": false
    }
  }
}
```

The response is the complete updated `KeymapResource`.

### `keymap.reset`

Removes every user override:

```json
{
  "expected_revision": 7
}
```

The response is the complete updated `KeymapResource`.

## Concurrency And Events

Every mutation requires the most recently installed `revision`. A stale mutation
returns an `invalid_request` error. On that error, request `keymap.get`, install the
new resource, and ask the user to retry if their pending edit still applies.

After a successful mutation the bridge emits:

```ts
Envelope & {
  type: 'event'
  name: 'keymap.changed'
  payload: KeymapResource
}
```

Install a resource only when its revision is newer than the current revision. Treat
an equal revision as an idempotent duplicate and ignore an older revision.

## Settings UI Guidance

An overlay settings panel should render the effective bindings grouped by context.
For each row:

- show the trigger and target;
- indicate whether it is inherited, overridden, added, or disabled;
- capture a replacement trigger from a real `keydown`;
- allow choosing a backend command from the session command catalog;
- allow choosing a frontend UI action from the frontend action registry;
- provide "Disable" using `keymap.override.set` with `target: null`;
- provide "Restore default" using `keymap.override.remove`;
- provide a global "Reset all shortcuts" using `keymap.reset`.

The frontend should check for duplicate effective triggers in a context before
submitting an edit and present a conflict choice. The backend identifies overrides by
trigger and therefore replacing a conflicting trigger is an explicit operation.

## Required Frontend Removal

Remove:

- the old `Record<context, Record<key string, command string>>` schema;
- key-combination string parsing;
- `routeKeymapAction`;
- fake `move` and `inputMode` command parsing;
- mixed semicolon local/backend action chains;
- assumptions about `SequenceView` as the context name.

Replace them with runtime validation of `KeymapResource`, exact trigger matching, and
typed target dispatch.
