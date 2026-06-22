# Command catalog

Commands are parsed into `CommandInvocation` values and bound through
`CommandCatalog` before any handler executes. Binding produces either an
`ExecutableCommand` with a required executor or a `RepeatPrevious` marker.

Each executable command declares an execution role and repeat policy. Runtime
commands default to repeat-eligible, but only commands that actually change the
engine become part of the next `again` target.

A submitted chain executes through `CommandTransaction`. Read-only resources remain
references to authoritative state; project, library, and workspace candidates are
copied only on first mutation. Handlers receive policy-scoped capabilities and cannot
access `PluginState`, timeline history, repeat state, or command sessions.

Clipboard and measure writes are collected per submission. Pending writes are visible
to later commands in the same chain. Candidates and history are prepared first,
effects are applied with rollback, and backend candidates are installed through the
no-fail transaction commit path.

The immutable presentation catalog is delivered in `session.hello` with catalog schema
version `1`. Completion and ranking are frontend-local; the backend remains
authoritative for strict parsing, binding, validation, and execution.
