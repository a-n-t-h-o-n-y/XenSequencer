#include "command_catalog_specs_internal.hpp"

#include <string>
#include <type_traits>
#include <utility>

#include <sequence/modify.hpp>

#include <xen/actions.hpp>
#include <xen/command_dsl.hpp>
#include <xen/message_level.hpp>

#include "actions_internal.hpp"

namespace xen::catalog_detail
{
namespace
{

constexpr auto targeted_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  TargetRequirement::CellOrElement,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

} // namespace

void append_edit_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"note"}, false, "Create a note at the current selection.",
        targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "pitch", 0),
                        optional_arg<float>("Float", "velocity", 100.f / 127.f),
                        optional_arg<float>("Float", "delay", 0.f),
                        optional_arg<float>("Float", "gate", 1.f)),
        [](CommandHandlerContext &context, CommandInvocation const &, int pitch,
           float velocity, float delay, float gate) {
            auto state = context.project();
            state = increment_state(
                std::move(state), require_selection(context.execution),
                [](auto selected, int note_pitch, float note_velocity, float note_delay,
                   float note_gate) {
                    auto note = sequence::modify::note(note_pitch, note_velocity,
                                                       note_delay, note_gate);
                    using Selected = std::decay_t<decltype(selected)>;
                    if constexpr (std::is_same_v<Selected, sequence::Cell>)
                    {
                        selected.elements.push_back(std::move(note));
                        return selected;
                    }
                    else
                    {
                        return note;
                    }
                },
                pitch, velocity, delay, gate);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Note Created"), context.execution);
        }));

    specs.push_back(command(
        {"delete"}, false, "Delete the current selection.", {"remove"},
        targeted_edit_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const mutation =
                action::delete_cell(state, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Deleted Selection"), mutation.selection);
        }));

    specs.push_back(
        command({"split"}, false, "Split the current selection.", targeted_edit_policy,
                std::make_tuple(optional_arg<std::size_t>("Unsigned", "count", 2)),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t count) {
                    auto state = context.project();
                    state = increment_state(
                        std::move(state), require_selection(context.execution),
                        [](auto target, std::size_t repeat_count) {
                            return sequence::modify::repeat(target, repeat_count);
                        },
                        count);
                    context.edit_project() = std::move(state);
                    return unchanged_selection_result(
                        minfo("Split Selection " + std::to_string(count) + " Times"),
                        context.execution);
                }));

    specs.push_back(command(
        {"lift"}, false, "Lift the current selection up one level.",
        targeted_edit_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const mutation =
                action::lift(state, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Lifted One Layer"), mutation.selection);
        }));
}

} // namespace xen::catalog_detail
