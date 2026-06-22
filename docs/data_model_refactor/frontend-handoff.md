# Backend Data-Model Refactor: Frontend Handoff

The backend now exposes the final breaking contract. The sibling frontend must migrate
without expecting compatibility aliases or mixed snapshots.

## Session startup

`session.hello` remains a request/response using protocol `xen.bridge.v1`. Its response
payload contains session metadata only:

```json
{
  "protocol": "xen.bridge.v1",
  "plugin_version": "...",
  "project_schema_version": 1,
  "library_schema_version": 1,
  "catalog": { "schema_version": 1, "commands": [] },
  "keymap": {}
}
```

It contains no project or library resource. Request `state.get` and `library.get`
after a successful hello. The removed `keymap.get` endpoint must not be called.

## Project resource

`state.get`, `state.changed`, and `command.execute.payload.snapshot` use:

```json
{
  "schema_version": 1,
  "project_revision": 42,
  "history_entry_id": 17,
  "project": {
    "measure": {
      "cell": { "weight": 1.0, "elements": [] },
      "time_signature": { "numerator": 4, "denominator": 4 }
    },
    "pitch": {
      "tuning": {
        "name": "12-TET",
        "definition": {
          "intervals": [0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100],
          "octave": 1200
        }
      },
      "scale": null,
      "transposition": 0,
      "translation_direction": "up",
      "base_frequency": 440.0
    }
  }
}
```

When active, `pitch.scale` is:

```json
{
  "source_id": "major-diatonic",
  "definition": {
    "name": "major diatonic",
    "tuning_length": 12,
    "intervals": [2, 2, 1, 2, 2, 2, 1],
    "mode": 1
  }
}
```

`source_id` may be null for an embedded scale. Musical behavior always comes from the
embedded definition. The removed flat names are `engine`, `tuning`, `tuning_name`,
`scale`, `key`, and `scale_translate_direction`.

The host-state JSON schema is also version 1:

```json
{ "schema": 1, "project": { "...": "same grouped project shape" } }
```

Old host-state data is rejected.

## Library resource

`library.get` and `library.changed` contain library data only:

```json
{
  "schema_version": 1,
  "library_revision": 9,
  "paths": {
    "library": "...",
    "sequences": "...",
    "tunings": "..."
  },
  "measures": [],
  "tunings": [],
  "scales": [
    {
      "id": "chromatic",
      "name": "chromatic",
      "definition": null,
      "intervals": [],
      "command": "set scale \"chromatic\""
    },
    {
      "id": "major-diatonic",
      "definition": {
        "name": "major diatonic",
        "tuning_length": 12,
        "intervals": [2, 2, 1, 2, 2, 2, 1],
        "mode": 1
      },
      "command": "set scale \"major-diatonic\""
    }
  ],
  "chords": [],
  "commands": {
    "reload_scales": "load scales",
    "reload_chords": "load chords",
    "library_directory": "libraryDirectory"
  }
}
```

Scale commands use stable IDs, not display names. `set scale chromatic` clears the
active scale. Mode changes preserve `source_id`; cycling fails when an active scale has
no source ID or its source no longer exists.

## Revisions and ingestion order

Project and library revisions are independent monotonic domains.

1. Complete `session.hello`.
2. Fetch `state.get` and `library.get`.
3. Install each resource through its own revision-aware store operation.
4. Apply `state.changed` only when its project revision is newer. Equal revisions are
   acceptable only for an initial or idempotent response.
5. Apply `library.changed` using the same rule against `library_revision`.
6. Apply `command.execute.payload.snapshot` through the same project ingestion path as
   `state.get` and `state.changed`.
7. Reconcile frontend-owned selection only after the complete project snapshot is
   installed.

Library reloads advance `library_revision` even when values are equal. Workspace path
changes also advance it. Failed and no-op project candidates produce no
`state.changed`. Transport events remain independent and carry no project or library
revision.

Frontend selection, focus, input mode, panels, command text, zoom, and scroll remain
frontend-owned and must not be overwritten by backend snapshots.
