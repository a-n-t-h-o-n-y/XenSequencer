# Unified Command Catalog Design

Purpose: replace `xen_command_tree` with a single declarative command catalog that powers:

1. Runtime command execution
2. Typed argument parsing (auto parsing with defaults)
3. Command bar completion/guide text
4. Command docs/reference generation

This keeps command behavior, metadata, and parsing rules in one place.

## Requirements

- One source of truth per command
- No runtime fallback path to legacy command execution
- Keep current chain semantics (`;` splitting, stop on first error, `again` behavior)
- Keep deprecated commands (`focus`, `show`, `load keys`, `set theme`) as warning no-ops for now
- Preserve explicit `ExecutionContext` flow
- Make adding a command feel like old `signature(...)` ergonomics

## Non-Goals

- Rewriting chain parsing (`parse_command_chain`) from scratch
- Changing command semantics during migration
- Introducing compatibility shims once fully switched

## High-Level Architecture

Flow at runtime:

1. `parse_command_chain(raw)` produces `CommandInvocation` values (existing)
2. Catalog resolves each invocation to a command spec by id path
3. Generic arg binder parses args based on command schema
4. Binder builds typed payload for that command
5. Command executor runs command handler with payload + `ExecutionContext`
6. Same catalog metadata serves completion/guide text/docs

No command tree dispatch in runtime path.

## Core Types (Proposed)

### Command Id Path

- `CommandPath`: tokenized command id (examples: `["set", "key"]`, `["move", "left"]`, `["version"]`)
- Match rule: longest exact path wins

### Arg Schema

- `ArgType`: enum for doc/completion metadata (`Int`, `Float`, `String`, `Bool`, `Pattern`, `Variant`, etc.)
- `ArgCardinality`: required, optional-with-default
- `ArgSpec` fields:
  - `name`
  - `type`
  - `required`
  - `default_text` (for guide/docs)
  - `parse_fn` (string -> typed value/error)

`parse_fn` is generated from existing `parse<T>()` adapters when possible.

### Command Spec

Each command spec contains:

- `path` (`CommandPath`)
- `description`
- `arg_specs` (ordered list)
- `flags`:
  - `accepts_pattern_prefix`
  - `deprecated_noop`
- `bind_fn`: generic binder uses `arg_specs` to parse tokens and construct typed payload
- `apply_fn`: applies typed payload to `PluginState` + `ExecutionContext` and returns `CommandActionResult`

### Catalog

- `CommandCatalog`: immutable registry of all `CommandSpec`
- Offers:
  - `resolve(invocation) -> ResolvedCommand | error`
  - `complete(partial_input) -> guide/completion text`
  - `docs() -> command docs list`

## Authoring API (Command Definition Ergonomics)

Goal: one concise definition per command, similar to old `signature` ergonomics.

Each command should be declared once with:

1. Path (id tokens)
2. Typed args + defaults
3. Description
4. Typed handler

The handler should receive already-parsed typed values (no manual token indexing).

Expected authoring experience:

1. Add command spec entry
2. Implement handler body
3. Done (runtime + completion + docs automatically updated)

## Parsing/Binder Behavior

For a resolved spec:

1. Remove matched id tokens from invocation words
2. Parse remaining words against `arg_specs` in order
3. Use defaults for missing optional args
4. Fail with structured parse error for missing/invalid args
5. Build typed payload
6. Return bound command

Error message policy:

- Unknown command: `Command not found: <first token>`
- Missing arg: consistent message from binder (`Missing argument: <name>`)
- Invalid arg: consistent message with arg name + parse reason

## Execution Model

Processor remains chain orchestrator:

1. Parse chain
2. Resolve/bind each segment via catalog
3. Execute bound command in order
4. Stop on first error
5. Apply commit policy centrally
6. Persist replay chain for `again`

`again` should replay the previously executed canonical/bound chain, not route through legacy systems.

## Completion/Guide Text From Same Catalog

Replace `XenCommandTree` completion with catalog-driven completion:

1. Tokenize partial input (existing quoting/pattern-aware helpers)
2. Resolve partial path
3. If path incomplete: suggest matching next id tokens
4. If path complete: show remaining args from `arg_specs` (`[name:type=default]` style)

This removes duplicated command metadata and keeps completion in sync with runtime.

## File Layout (Proposed)

New:

- `include/xen/command_catalog.hpp`
  - `CommandCatalog`, `CommandSpec`, `ArgSpec` interfaces
- `include/xen/command_catalog_types.hpp`
  - shared types for binder/parse results/errors
- `src/command_catalog.cpp`
  - catalog construction and lookup
- `src/command_catalog_bind.cpp`
  - generic typed arg binding
- `src/command_catalog_complete.cpp`
  - completion/guide text generation
- `src/command_catalog_docs.cpp`
  - docs/reference output
- `src/command_definitions/*.cpp`
  - grouped command specs (transport/edit/pitch/library/etc.)

Retired (completed):

- `src/xen_command_tree.cpp` (removed)
- `include/xen/xen_command_tree.hpp` (removed)
- legacy `CommandBase`/`CommandGroup` execution usage

## Migration Plan

### Phase A: Infrastructure

- Add catalog core types, resolver, binder, completion engine
- Keep existing typed execution handler bodies
- Add parity tests for completion output shape

### Phase B: Port Definitions

- Port commands into catalog definitions by domain
- Bind each command to existing typed handler behavior
- Keep deprecated no-op commands as catalog entries

### Phase C: Switch Call Sites

- Runtime: `XenProcessor` uses catalog resolve/bind directly
- UI: guide/completion requests use catalog, not `command_tree`
- Docs: command reference generated from catalog

### Phase D: Remove Legacy (Completed)

- Deleted `xen_command_tree` and command class execution framework
- Remove dual metadata paths
- Keep only catalog path

## Test Plan

Must-have tests:

1. Catalog resolve:
   - exact match
   - unknown command
   - longest-path disambiguation
2. Binder:
   - required args
   - defaults
   - parse failures
   - quoted args
3. Processor integration:
   - chain stop-on-error
   - prior mutations preserved on later error
   - `again` replay still correct
4. Completion:
   - id token completion
   - arg guide text from schema
   - deprecated commands still listed (until removed)
5. Docs:
   - command reference generation uses catalog only

## Definition Of Done

- Adding a command requires one definition site only
- Runtime execution, completion, and docs all derive from that same definition
- `xen_command_tree` is removed
- No fallback execution path remains
