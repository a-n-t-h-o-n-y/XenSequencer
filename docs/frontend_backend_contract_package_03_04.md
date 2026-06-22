# Frontend follow-up for backend data model refactor packages 03, 04, 05, and 08

This backend change set is intentionally breaking. `../xen-frontend` was not modified in
this task. The frontend must adopt the contract below before it can interoperate with
the updated backend.

## Summary

- Backend no longer owns active selection or input mode.
- Snapshot schema version is now `6`.
- Snapshot payload no longer contains `editor`.
- `command.execute` request context must now include frontend-owned
  `selection` for targeted commands and `expected_project_revision` for every
  project-aware command.
- `command.execute` response now includes nullable `suggested_selection`.
- Backend `move ...` and `inputMode ...` commands were removed.
- Undo and redo remain command names, but are now executed as processor-owned
  history navigation steps instead of normal handlers.
- Timeline commits now use explicit `commit(project)` semantics internally.
- `session.hello` now includes the immutable command catalog with catalog schema
  version `1`.
- `catalog.get`, `command.complete`, `command.completeText`, and
  `command.completeId` were removed.
- Command completion, filtering, ranking, and tolerant active-segment tokenization are
  now frontend-local responsibilities.

### `session.hello`

The response now carries catalog and keymap data directly:

```json
{
  "protocol": "xen.bridge.v1",
  "snapshot_schema_version": 6,
  "plugin_version": "...",
  "catalog": {
    "schema_version": 1,
    "commands": [
      {
        "path": ["set", "key"],
        "description": "Set transposition key.",
        "accepts_pattern_prefix": false,
        "target_requirement": "none",
        "arguments": [
          {
            "kind": "integer",
            "display_name": "key",
            "required": false,
            "default_value": "0",
            "constraints": []
          }
        ]
      }
    ]
  },
  "keymap": {}
}
```

Stable argument kinds are presentation-safe identifiers rather than C++ type names.
Backend-only command policies and handlers are not serialized. The old duplicate
`reference.commands` representation is absent; help UI must derive command signatures
from the catalog.

Removed requests receive the ordinary `invalid_request` response for an unknown
request name. The frontend should cache the handshake catalog and perform completion
locally without bridge requests on each keystroke.

## Bridge protocol changes

### `state.get` and `state.changed`

Snapshot payload:

```json
{
  "schema_version": 6,
  "snapshot_version": 123,
  "history_entry_id": 456,
  "project_revision": 789,
  "engine": { "...": "unchanged engine payload" },
  "library": { "...": "unchanged library payload" }
}
```

Removed fields:

- `editor`
- `editor.selected`
- `editor.input_mode`

Frontend responsibilities:

- Keep selection path in frontend state.
- Keep input mode in frontend state.
- Reconcile selection to the root path `[]` whenever a fresh snapshot makes the
  stored path invalid.

### `command.execute` request

Targeted commands now require:

```json
{
  "command": "set pitch 7",
  "context": {
    "expected_project_revision": 789,
    "selection": {
      "path": [
        { "kind": "element", "index": 0 },
        { "kind": "cell", "index": 2 },
        { "kind": "element", "index": 1 }
      ]
    }
  }
}
```

Selection path encoding rules:

- `kind: "element"` means a cell element index.
- `kind: "cell"` means a child-cell index within the currently selected sequence.
- The path alternates element, cell, element, cell, ...
- Root cell selection is `{"path":[]}`.

Validation behavior:

- Missing selection on a targeted command -> backend error.
- Invalid path kind or non-unsigned index -> bridge `invalid_request`.
- Unresolvable path -> command error.
- Wrong target kind -> command error.
- Stale `expected_project_revision` -> command error before selection resolution.

### `command.execute` response

Responses now include:

```json
{
  "status": {
    "level": "info",
    "message": "Selection Duplicated"
  },
  "suggested_selection": {
    "path": [
      { "kind": "element", "index": 1 }
    ]
  },
  "snapshot": { "...": "schema v6 snapshot" }
}
```

`suggested_selection` may be `null`.

Frontend responsibilities:

- If `suggested_selection` is non-null, adopt it as the new frontend selection.
- If it is `null`, keep the existing frontend selection unless the new snapshot
  invalidates it, in which case fall back to `[]`.

## Removed backend commands

These command IDs no longer exist:

- `move left`
- `move right`
- `move up`
- `move down`
- `inputMode ...`

Frontend must replace them with local actions:

- local selection traversal using the existing typed path semantics
- local input-mode switching

## Selection suggestion rules now implemented by backend

- `duplicate` -> duplicated cell or duplicated element
- `delete` / `cut` -> nearest surviving sibling, otherwise parent
- `paste` of a cell over an element -> parent cell
- successful non-structural targeted edits -> supplied selection

The frontend should treat these suggestions as authoritative for post-command
selection updates.

## History / snapshot state the frontend should track

Keep both in frontend UI state:

- `history_entry_id`
- `project_revision`

Use `project_revision` for `command.execute.context.expected_project_revision`.

`history_entry_id` still matters for frontend display and for any future
history-aware UX, but it is no longer mixed with backend editor session state.

## Frontend migration checklist

- Update snapshot parser to schema `6`.
- Remove any expectation that snapshots include `editor`.
- Store selection path locally.
- Store input mode locally.
- Send `context.selection` for all targeted commands.
- Send `context.expected_project_revision` for all project-aware commands.
- Consume `suggested_selection` from command responses.
- Reimplement navigation keybindings locally.
- Reimplement input-mode keybindings locally.
- Reconcile invalid frontend selection back to `[]` on snapshot updates.
- Accept catalog schema version `1` from `session.hello`.
- Derive command help and completion from `catalog.commands`.
- Remove calls to `catalog.get` and all `command.complete*` requests.
- Tolerantly tokenize only the active semicolon-delimited chain segment locally;
  continue submitting complete command text to the strict backend parser.
