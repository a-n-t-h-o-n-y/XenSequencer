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
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::None,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::None, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

auto require_row(Composition &composition, std::size_t row) -> CompositionRow &
{
    if (row >= composition.rows.size())
    {
        throw std::out_of_range{"Composition row index is out of range."};
    }
    return composition.rows[row];
}

auto require_column(Composition &composition, std::size_t column) -> CompositionColumn &
{
    if (column >= composition.columns.size())
    {
        throw std::out_of_range{"Composition column index is out of range."};
    }
    return composition.columns[column];
}

auto require_sequence_entry(SequenceBank &bank, SequenceId id) -> SequenceBankEntry &
{
    auto const at = std::ranges::find(bank.sequences, id, &SequenceBankEntry::id);
    if (at == bank.sequences.end())
    {
        throw std::invalid_argument{"Sequence ID does not exist."};
    }
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
    {
        throw std::invalid_argument{std::string{kind} + " name must not be empty."};
    }
}

auto find_sequence_by_name(SequenceBank &bank, std::string const &name)
    -> SequenceBankEntry *
{
    for (auto &entry : bank.sequences)
    {
        if (sequence_names_equal(effective_sequence_name(entry), name))
        {
            return &entry;
        }
    }
    return nullptr;
}

auto create_named_sequence(SequenceBank &bank, std::string name, sequence::Cell cell)
    -> SequenceId
{
    if (find_sequence_by_name(bank, name) != nullptr)
    {
        throw std::invalid_argument{"Sequence name is already in use."};
    }
    auto const id = create_sequence(bank, std::move(cell));
    require_sequence_entry(bank, id).name = std::move(name);
    return id;
}

auto assign_cell_by_sequence_name(ProjectState &state, std::size_t row,
                                  std::size_t column, std::string name) -> void
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
    {
        assign_sequence_reference(state.composition, row, column, current.id);
        return;
    }

    auto const id =
        create_named_sequence(state.sequence_bank, std::move(name), current.cell);
    assign_sequence_reference(state.composition, row, column, id);
}

auto require_channel_id(ChannelId const &channel_id) -> void
{
    if (channel_id.empty())
    {
        throw std::invalid_argument{"Channel ID must not be empty."};
    }
}

auto row_insert_index(std::size_t row, bool after) -> std::size_t
{
    return row + (after ? 1U : 0U);
}

auto column_insert_index(std::size_t column, bool after) -> std::size_t
{
    return column + (after ? 1U : 0U);
}

} // namespace

void append_composition_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(
        command({"composition", "loop", "start"}, false,
                "Set the composition loop start column.", composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("column_index")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t column_index) {
                    auto state = context.project();
                    set_loop_start(state.composition, column_index);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Loop Start Set: " +
                                             std::to_string(column_index)));
                }));

    specs.push_back(command({"composition", "loop", "end"}, false,
                            "Set the composition loop end column.",
                            composition_edit_policy,
                            std::make_tuple(required_arg<std::size_t>("column_index")),
                            [](CommandHandlerContext &context,
                               CommandInvocation const &, std::size_t column_index) {
                                auto state = context.project();
                                set_loop_end(state.composition, column_index);
                                context.edit_project() = std::move(state);
                                return make_result(minfo("Composition Loop End Set: " +
                                                         std::to_string(column_index)));
                            }));

    specs.push_back(command(
        {"composition", "row", "insert", "before"}, false,
        "Insert a composition row before the target row.", composition_edit_policy,
        std::make_tuple(required_arg<std::size_t>("row_index")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t row_index) {
            auto state = context.project();
            auto const channel_id =
                require_row(state.composition, row_index).channel_id;
            insert_row(state.composition, row_insert_index(row_index, false),
                       channel_id);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Row Inserted."));
        }));

    specs.push_back(command(
        {"composition", "row", "insert", "after"}, false,
        "Insert a composition row after the target row.", composition_edit_policy,
        std::make_tuple(required_arg<std::size_t>("row_index")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t row_index) {
            auto state = context.project();
            auto const channel_id =
                require_row(state.composition, row_index).channel_id;
            insert_row(state.composition, row_insert_index(row_index, true),
                       channel_id);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Row Inserted."));
        }));

    specs.push_back(command({"composition", "row", "delete"}, false,
                            "Delete a composition row.", composition_edit_policy,
                            std::make_tuple(required_arg<std::size_t>("row_index")),
                            [](CommandHandlerContext &context,
                               CommandInvocation const &, std::size_t row_index) {
                                auto state = context.project();
                                remove_row(state.composition, row_index);
                                context.edit_project() = std::move(state);
                                return make_result(minfo("Composition Row Deleted."));
                            }));

    specs.push_back(
        command({"composition", "row", "rename"}, false, "Rename a composition row.",
                composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("row_index"),
                                required_arg<std::string>("name")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t row_index, std::string const &name) {
                    require_name(name, "Composition row");
                    auto state = context.project();
                    require_row(state.composition, row_index).name = name;
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Row Renamed."));
                }));

    specs.push_back(
        command({"composition", "row", "channel"}, false,
                "Set a composition row channel ID.", composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("row_index"),
                                required_arg<std::string>("channel_id")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t row_index, std::string const &channel_id) {
                    require_channel_id(channel_id);
                    auto state = context.project();
                    assign_row_channel(state.composition, row_index, channel_id);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Row Channel Set."));
                }));

    specs.push_back(
        command({"composition", "column", "insert", "before"}, false,
                "Insert a composition column before the target column.",
                composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("column_index")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t column_index) {
                    auto state = context.project();
                    auto const column = require_column(state.composition, column_index);
                    insert_column(state.composition,
                                  column_insert_index(column_index, false), column);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Column Inserted."));
                }));

    specs.push_back(command(
        {"composition", "column", "insert", "after"}, false,
        "Insert a composition column after the target column.", composition_edit_policy,
        std::make_tuple(required_arg<std::size_t>("column_index")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t column_index) {
            auto state = context.project();
            auto const column = require_column(state.composition, column_index);
            insert_column(state.composition, column_insert_index(column_index, true),
                          column);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Column Inserted."));
        }));

    specs.push_back(command({"composition", "column", "delete"}, false,
                            "Delete a composition column.", composition_edit_policy,
                            std::make_tuple(required_arg<std::size_t>("column_index")),
                            [](CommandHandlerContext &context,
                               CommandInvocation const &, std::size_t column_index) {
                                auto state = context.project();
                                remove_column(state.composition, column_index);
                                context.edit_project() = std::move(state);
                                return make_result(
                                    minfo("Composition Column Deleted."));
                            }));

    specs.push_back(
        command({"composition", "column", "duplicate"}, false,
                "Duplicate a composition column.", composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("column_index")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t column_index) {
                    auto state = context.project();
                    duplicate_column(state.composition, column_index, column_index + 1);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Column Duplicated."));
                }));

    specs.push_back(command(
        {"composition", "cell", "assign"}, false,
        "Assign a sequence to a composition cell.", composition_edit_policy,
        std::make_tuple(required_arg<std::size_t>("row_index"),
                        required_arg<std::size_t>("column_index"),
                        required_arg<std::string>("sequence_name")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t row_index, std::size_t column_index,
           std::string const &sequence_name) {
            auto state = context.project();
            assign_cell_by_sequence_name(state, row_index, column_index, sequence_name);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Cell Assigned."));
        }));

    specs.push_back(command(
        {"composition", "cell", "clear"}, false, "Clear a composition cell.",
        composition_edit_policy,
        std::make_tuple(required_arg<std::size_t>("row_index"),
                        required_arg<std::size_t>("column_index")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t row_index, std::size_t column_index) {
            auto state = context.project();
            clear_sequence_reference(state.composition, row_index, column_index);
            context.edit_project() = std::move(state);
            return make_result(minfo("Composition Cell Cleared."));
        }));
}

} // namespace xen::catalog_detail
