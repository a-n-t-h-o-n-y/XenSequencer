# Frontend-local command completion

Backend completion endpoints have been removed. The frontend receives the immutable
command catalog once in `session.hello.payload.catalog` and owns tolerant tokenization,
filtering, ranking, fuzzy matching, and display.

The frontend migration in `../xen-frontend` must:

- require catalog schema version `1`;
- cache `catalog.commands`;
- tokenize only the active semicolon-delimited command-chain segment;
- use stable argument `kind`, `display_name`, `required`, `default_value`, and
  `constraints` metadata;
- derive command help from the same catalog;
- submit final text to `command.execute`, where strict backend parsing, binding, and
  semantic validation remain authoritative.

Do not call `catalog.get`, `command.complete`, `command.completeText`, or
`command.completeId`; those requests now return the standard unknown-request error.
