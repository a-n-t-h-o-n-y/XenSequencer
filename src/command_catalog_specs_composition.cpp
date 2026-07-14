#include "command_catalog_specs_internal.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

#include <xen/command_dsl.hpp>
#include <xen/composition.hpp>
#include <xen/message_level.hpp>

namespace xen::catalog_detail
{
namespace
{

constexpr auto composition_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  CopyBufferAccess::None,
                  TargetRequirement::None,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

auto require_sequence_entry(SequenceBank &bank, SequenceId id) -> SequenceBankEntry &
{
    auto const at = std::ranges::find(bank.sequences, id, &SequenceBankEntry::id);
    if (at == bank.sequences.end())
        throw std::invalid_argument{"Sequence ID does not exist."};
    return *at;
}

auto fallback_sequence_name(SequenceId id) -> std::string
{
    return "S" + std::to_string(id);
}

auto effective_sequence_name(SequenceBankEntry const &entry) -> std::string
{
    return entry.name.value_or(fallback_sequence_name(entry.id));
}

auto sequence_name_key(std::string const &name) -> std::string
{
    auto key = name;
    std::ranges::transform(key, key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

auto sequence_names_equal(std::string const &lhs, std::string const &rhs) -> bool
{
    return sequence_name_key(lhs) == sequence_name_key(rhs);
}

auto require_name(std::string const &name, char const *kind) -> void
{
    if (name.empty())
        throw std::invalid_argument{std::string{kind} + " name must not be empty."};
}

auto find_sequence_by_name(SequenceBank &bank, std::string const &name)
    -> SequenceBankEntry *
{
    for (auto &entry : bank.sequences)
    {
        if (sequence_names_equal(effective_sequence_name(entry), name))
            return &entry;
    }
    return nullptr;
}

auto create_named_sequence(SequenceBank &bank, std::string name, sequence::Cell cell)
    -> SequenceId
{
    if (find_sequence_by_name(bank, name) != nullptr)
        throw std::invalid_argument{"Sequence name is already in use."};
    auto const id = create_sequence(bank, std::move(cell));
    require_sequence_entry(bank, id).name = std::move(name);
    return id;
}

auto assign_cell_by_sequence_name(ProjectState &state, CompositionCoordinate row,
                                  CompositionCoordinate column, std::string name)
    -> void
{
    require_name(name, "Sequence");
    auto const current_id = sequence_reference_at(state.composition, row, column);
    auto *named = find_sequence_by_name(state.sequence_bank, name);
    if (named != nullptr)
    {
        assign_sequence_reference(state.composition, row, column, named->id);
        return;
    }

    if (!current_id.has_value())
    {
        auto const id = create_named_sequence(state.sequence_bank, std::move(name),
                                              sequence::Cell{});
        assign_sequence_reference(state.composition, row, column, id);
        return;
    }

    auto &current = require_sequence_entry(state.sequence_bank, *current_id);
    if (sequence_names_equal(effective_sequence_name(current), name))
        return;

    auto const id =
        create_named_sequence(state.sequence_bank, std::move(name), current.cell);
    assign_sequence_reference(state.composition, row, column, id);
}

} // namespace

void append_composition_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"composition", "loop", "start"}, false,
        "Set the composition loop start column.", composition_edit_policy,
        std::make_tuple(required_arg<CompositionCoordinate>("column_coordinate")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           CompositionCoordinate column) {
            auto state = context.project();
            set_loop_start(state.composition, column);
            context.edit_project() = std::move(state);
            return make_result(
                minfo("Composition Loop Start Set: " + std::to_string(column)));
        }));

    specs.push_back(command(
        {"composition", "loop", "end"}, false, "Set the composition loop end column.",
        composition_edit_policy,
        std::make_tuple(required_arg<CompositionCoordinate>("column_coordinate")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           CompositionCoordinate column) {
            auto state = context.project();
            set_loop_end(state.composition, column);
            context.edit_project() = std::move(state);
            return make_result(
                minfo("Composition Loop End Set: " + std::to_string(column)));
        }));

    specs.push_back(
        command({"composition", "row", "rename"}, false, "Rename a composition row.",
                composition_edit_policy,
                std::make_tuple(required_arg<CompositionCoordinate>("row_coordinate"),
                                required_arg<std::string>("name")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   CompositionCoordinate row, std::string const &name) {
                    require_name(name, "Composition row");
                    auto state = context.project();
                    composition_row(state.composition, row).name = name;
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Row Renamed."));
                }));

    specs.push_back(
        command({"composition", "row", "channel"}, false,
                "Set a composition row channel ID.", composition_edit_policy,
                std::make_tuple(required_arg<CompositionCoordinate>("row_coordinate"),
                                required_arg<std::string>("channel_id")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   CompositionCoordinate row, std::string const &channel_id) {
                    auto state = context.project();
                    assign_row_channel(state.composition, row, channel_id);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Row Channel Set."));
                }));

    specs.push_back(command(
        {"composition", "cell", "assign"}, false,
        "Assign a sequence to a composition cell.", composition_edit_policy,
        std::make_tuple(required_arg<CompositionCoordinate>("row_coordinate"),
                        required_arg<CompositionCoordinate>("column_coordinate"),
                        required_arg<std::string>("sequence_name")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           CompositionCoordinate row, CompositionCoordinate column,
           std::string const &sequence_name) {
            auto state = context.project();
            assign_cell_by_sequence_name(state, row, column, sequence_name);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Cell Assigned."));
        }));

    specs.push_back(command(
        {"composition", "cell", "unassign"}, false,
        "Unassign a sequence from a composition cell.", composition_edit_policy,
        std::make_tuple(required_arg<CompositionCoordinate>("row_coordinate"),
                        required_arg<CompositionCoordinate>("column_coordinate")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           CompositionCoordinate row, CompositionCoordinate column) {
            auto state = context.project();
            unassign_sequence_reference(state.composition, row, column);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Cell Unassigned."));
        }));

    specs.push_back(
        command({"sequence", "clear"}, false, "Clear the active sequence's contents.",
                composition_edit_policy, std::make_tuple(),
                [](CommandHandlerContext &context, CommandInvocation const &) {
                    auto state = context.project();
                    selected_sequence(state, context.execution.cursor).elements.clear();
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Sequence Cleared"), SelectionPath{});
                }));

    specs.push_back(
        command({"composition", "cell", "move"}, false, "Move a composition cell.",
                composition_edit_policy,
                std::make_tuple(required_arg<CompositionCoordinate>("from_row"),
                                required_arg<CompositionCoordinate>("from_column"),
                                required_arg<CompositionCoordinate>("to_row"),
                                required_arg<CompositionCoordinate>("to_column")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   CompositionCoordinate from_row, CompositionCoordinate from_column,
                   CompositionCoordinate to_row, CompositionCoordinate to_column) {
                    auto state = context.project();
                    move_sequence_reference(
                        state.composition,
                        {.row_coordinate = from_row, .column_coordinate = from_column},
                        {.row_coordinate = to_row, .column_coordinate = to_column});
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Cell Moved."));
                }));
}

} // namespace xen::catalog_detail
