#pragma once

#include <stdexcept>
#include <vector>

#include <xen/command_catalog_types.hpp>

#include "command_catalog_metadata_internal.hpp"

namespace xen::catalog_detail
{

void append_bootstrap_specs(std::vector<CommandSpec> &specs);
void append_composition_specs(std::vector<CommandSpec> &specs);
void append_edit_specs(std::vector<CommandSpec> &specs);
void append_midi_cc_specs(std::vector<CommandSpec> &specs);
void append_set_and_shift_specs(std::vector<CommandSpec> &specs);
void append_transform_specs(std::vector<CommandSpec> &specs);

[[nodiscard]] inline auto require_selection(CommandExecutionContext const &context)
    -> SelectionPath const &
{
    if (!context.selection.has_value())
    {
        throw std::runtime_error{"Selection is required."};
    }
    return *context.selection;
}

[[nodiscard]] inline auto make_result(CommandStatus status,
                                      std::optional<SelectionPath> suggested_selection =
                                          std::nullopt) -> CommandApplicationResult
{
    return CommandApplicationResult{
        .status = std::move(status),
        .suggested_selection = std::move(suggested_selection),
    };
}

[[nodiscard]] inline auto unchanged_selection_result(
    CommandStatus status, CommandExecutionContext const &context)
    -> CommandApplicationResult
{
    return make_result(std::move(status), context.selection);
}

} // namespace xen::catalog_detail
