# Command catalog

Commands are parsed into `CommandInvocation` values and bound through
`CommandCatalog` before any handler executes. Binding produces either an
`ExecutableCommand` with a required executor or a `RepeatPrevious` marker.

Each executable command declares an execution role and repeat policy. Runtime
commands default to repeat-eligible, but only commands that actually change the
engine become part of the next `again` target.

A submitted chain executes against a copied `PluginState`. Returned errors,
exceptions, bind failures, and external-effect failures discard that copy.
Successful engine changes create one timeline entry for the complete
submission; editor and library changes persist without creating engine history.

Clipboard and measure writes are collected per submission. Pending writes are
visible to later commands in the same chain, then prepared and applied before
the copied backend state is installed.

`complete_text` and `complete_id` remain as compatibility projections for the
webview bridge. Structured completion is the current catalog API.
