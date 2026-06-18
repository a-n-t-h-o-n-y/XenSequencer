# Command Catalog

The command bar is backed by an owned `xen::CommandCatalog`. Each
`XenProcessor` has its own catalog, and additional commands can be registered
before or during the processor lifetime.

Use the typed embedded DSL from `<xen/command_dsl.hpp>`:

```cpp
processor.command_catalog().add(xen::command_dsl::command(
    {"set", "customValue"},
    false,
    "Set an application-defined value.",
    std::make_tuple(
        xen::command_dsl::required_arg<int>("value")),
    [](xen::PluginState &plugin_state,
       xen::ExecutionContext,
       xen::CommandInvocation const &,
       int value) {
        auto state = plugin_state.timeline.get_state();
        state.sequencer.key = value;
        plugin_state.timeline.stage(std::move(state));
        return std::pair{
            xen::MessageLevel::Info,
            std::string{"Custom value set"},
        };
    }));
```

The definition supplies command-path hierarchy, documentation, typed argument
parsing, defaults, completion metadata, and executable behavior. The catalog
rejects duplicate paths, trailing arguments, pattern prefixes on commands that
do not accept them, duplicate argument names, and required positional
arguments following optional arguments.

Handlers receive already parsed arguments. Before invoking a handler, the DSL
stages the current execution context and resets commit intent. Afterward it
derives the resulting context, engine-mutation flag, and commit intent for the
processor's chain orchestrator.

`CommandCatalog::complete` returns all matching structured candidates and the
active argument. The existing `complete_text` and `complete_id` methods remain
as compatibility projections. The webview bridge exposes structured results
through `command.complete`.

The existing built-in commands currently use the typed-action adapter supplied
by the same DSL. New commands do not need to add a type to `CommandAction` or
modify the central action visitor.
