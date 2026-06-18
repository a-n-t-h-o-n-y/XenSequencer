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

enum class MoveDirection
{
    Left,
    Right,
    Up,
    Down,
};

auto move_handler(MoveDirection direction)
{
    return [direction](PluginState &ps, ExecutionContext context,
                       CommandInvocation const &, std::size_t amount) {
        auto state = ps.timeline.get_state();
        state.aux = context;
        auto label = std::string{};
        switch (direction)
        {
        case MoveDirection::Left:
            state.aux =
                action::move_left(state.sequencer, std::move(state.aux), amount);
            label = "Left";
            break;
        case MoveDirection::Right:
            state.aux =
                action::move_right(state.sequencer, std::move(state.aux), amount);
            label = "Right";
            break;
        case MoveDirection::Up:
            state.aux = action::move_up(state.sequencer, std::move(state.aux), amount);
            label = "Up";
            break;
        case MoveDirection::Down:
            state.aux =
                action::move_down(state.sequencer, std::move(state.aux), amount);
            label = "Down";
            break;
        }
        ps.timeline.stage(std::move(state));
        return mdebug("Moved " + label + " " + std::to_string(amount) + " Times");
    };
}

} // namespace

void append_edit_specs(std::vector<CommandSpec> &specs)
{
    auto const amount_args =
        std::make_tuple(optional_arg<std::size_t>("Unsigned", "amount", 1));
    specs.push_back(command({"move", "left"}, false, "Move selection left.",
                            amount_args, move_handler(MoveDirection::Left)));
    specs.push_back(command({"move", "right"}, false, "Move selection right.",
                            amount_args, move_handler(MoveDirection::Right)));
    specs.push_back(command({"move", "up"}, false, "Move selection up one level.",
                            amount_args, move_handler(MoveDirection::Up)));
    specs.push_back(command({"move", "down"}, false, "Move selection down one level.",
                            amount_args, move_handler(MoveDirection::Down)));

    specs.push_back(
        command({"note"}, false, "Create a note at the current selection.",
                std::make_tuple(optional_arg<int>("Int", "pitch", 0),
                                optional_arg<float>("Float", "velocity", 100.f / 127.f),
                                optional_arg<float>("Float", "delay", 0.f),
                                optional_arg<float>("Float", "gate", 1.f)),
                [](PluginState &ps, ExecutionContext context, CommandInvocation const &,
                   int pitch, float velocity, float delay, float gate) {
                    auto state = ps.timeline.get_state();
                    state.aux = context;
                    state = increment_state(
                        std::move(state),
                        [](auto selected, int note_pitch, float note_velocity,
                           float note_delay, float note_gate) {
                            auto note = sequence::modify::note(
                                note_pitch, note_velocity, note_delay, note_gate);
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
                    ps.timeline.stage(std::move(state));
                    return minfo("Note Created");
                }));

    specs.push_back(
        command({"delete"}, false, "Delete the current selection.", std::make_tuple(),
                [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
                    ps.timeline.stage(action::delete_cell(ps.timeline.get_state()));
                    return minfo("Deleted Selection");
                }));

    specs.push_back(
        command({"split"}, false, "Split the current selection.",
                std::make_tuple(optional_arg<std::size_t>("Unsigned", "count", 2)),
                [](PluginState &ps, ExecutionContext context, CommandInvocation const &,
                   std::size_t count) {
                    auto state = ps.timeline.get_state();
                    state.aux = context;
                    state = increment_state(
                        std::move(state),
                        [](auto target, std::size_t repeat_count) {
                            return sequence::modify::repeat(target, repeat_count);
                        },
                        count);
                    ps.timeline.stage(std::move(state));
                    return minfo("Split Selection " + std::to_string(count) + " Times");
                }));

    specs.push_back(command(
        {"lift"}, false, "Lift the current selection up one level.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
            ps.timeline.stage(action::lift(ps.timeline.get_state()));
            return minfo("Selection Lifted One Layer");
        }));
}

} // namespace xen::catalog_detail
