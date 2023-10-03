#pragma once

#include <optional>
#include <string>
#include <utility>

#include <sequence/sequence.hpp>
#include <signals_light/signal.hpp>

#include <xen/actions.hpp>
#include <xen/command.hpp>
#include <xen/input_mode.hpp>
#include <xen/message_type.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

inline auto on_focus_change_request = sl::Signal<void(std::string const &)>{};

inline auto copy_buffer = std::optional<sequence::Cell>{std::nullopt};

// All Command IDs should be in their normalized form, for strings this means
// all lower case.
inline auto const command_tree = cmd_group(
    "", ArgInfo<std::string>{"command_name"},

    cmd("undo",
        [](XenTimeline &tl) {
            return tl.undo() ? msuccess("Undo Successful") : mwarning("Can't Undo");
        }),

    cmd("redo",
        [](XenTimeline &tl) {
            return tl.redo() ? msuccess("Redo Successful") : mwarning("Can't Redo");
        }),

    cmd("copy",
        [](XenTimeline &tl) {
            auto copy = action::copy(tl);
            if (copy.has_value())
            {
                copy_buffer = std::move(*copy);
                return msuccess("Copied.");
            }
            return mwarning("Nothing to copy.");
        }),

    cmd("cut",
        [](XenTimeline &tl) {
            auto cut = action::cut(tl);
            if (!cut.has_value())
            {
                return mwarning("Nothing to cut.");
            }
            auto &[buffer, state] = *cut;
            tl.add_state(std::move(state));
            copy_buffer = std::move(buffer);
            return msuccess("Cut.");
        }),

    cmd("paste",
        [](XenTimeline &tl) {
            auto const result = action::paste(tl, copy_buffer);
            if (result.has_value())
            {
                tl.add_state(*result);
                return msuccess("Pasted.");
            }
            return mwarning("Nothing to paste.");
        }),

    cmd("duplicate",
        [](XenTimeline &tl) {
            auto [aux, state] = action::duplicate(tl);
            tl.set_aux_state(std::move(aux), false);
            tl.add_state(std::move(state));
            return msuccess("Duplicated.");
        }),

    cmd(
        "mode",
        [](XenTimeline &tl, InputMode mode) {
            tl.set_aux_state(action::set_mode(tl, mode));
            return msuccess("Changed mode to '" + to_string(mode) + "'.");
        },
        ArgInfo<InputMode>{"mode"}),

    cmd(
        "focus",
        [](auto &, std::string const &name) {
            on_focus_change_request(name);
            return msuccess("Focused on '" + name + "'.");
        },
        ArgInfo<std::string>{"component"}),

    // TODO create a direction enum
    cmd_group("move", ArgInfo<std::string>{"direction"},

              cmd("left",
                  [](XenTimeline &tl) {
                      tl.set_aux_state(action::move_left(tl));
                      return msuccess("Moved Left.");
                  }),

              cmd("right",
                  [](XenTimeline &tl) {
                      tl.set_aux_state(action::move_right(tl));
                      return msuccess("Moved Right.");
                  }),

              cmd("up",
                  [](XenTimeline &tl) {
                      tl.set_aux_state(action::move_up(tl));
                      // TODO message depending on if moved or hit ceiling
                      return msuccess("Moved Up.");
                  }),

              cmd("down",
                  [](XenTimeline &tl) {
                      tl.set_aux_state(action::move_down(tl));
                      // TODO message depending on if moved or hit floor
                      return msuccess("Moved Down.");
                  })),

    cmd(
        "addMeasure",
        [](XenTimeline &tl, sequence::TimeSignature const &ts) {
            auto [aux, state] = action::add_measure(tl, ts);
            tl.set_aux_state(std::move(aux), false);
            tl.add_state(std::move(state));
            return msuccess("Added measure.");
        },
        ArgInfo<sequence::TimeSignature>("duration", {{4, 4}})),

    pattern(cmd_group(
        "humanize", ArgInfo<InputMode>{"mode"},

        cmd(
            InputMode::Velocity,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                tl.add_state(action::humanize_velocities(tl, pattern, amount));
                return msuccess("Humanized Velocity.");
            },
            ArgInfo<float>{"amount", 0.1f}),

        cmd(
            InputMode::Delay,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                tl.add_state(action::humanize_delays(tl, pattern, amount));
                return msuccess("Humanized Delay.");
            },
            ArgInfo<float>{"amount", 0.1f}),

        cmd(
            InputMode::Gate,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                tl.add_state(action::humanize_gates(tl, pattern, amount));
                return msuccess("Humanized Gate.");
            },
            ArgInfo<float>{"amount", 0.1f}))),

    pattern(cmd_group(
        "randomize", ArgInfo<InputMode>{"mode"},
        // TODO implement pattern param in actions.hpp

        cmd(
            InputMode::Note,
            [](XenTimeline &tl, sequence::Pattern const &pattern, int min, int max) {
                tl.add_state(action::randomize_notes(tl, /*pattern,*/ min, max));
                return msuccess("Randomized Note.");
            },
            ArgInfo<int>{"min", -12}, ArgInfo<int>{"max", 12}),

        cmd(
            InputMode::Velocity,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                tl.add_state(action::randomize_velocities(tl, /*pattern,*/ min, max));
                return msuccess("Randomized Velocity.");
            },
            ArgInfo<float>{"min", 0.01f}, ArgInfo<float>{"max", 1.f}),

        cmd(
            InputMode::Delay,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                tl.add_state(action::randomize_delays(tl, /*pattern,*/ min, max));
                return msuccess("Randomized Delay.");
            },
            ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}),

        cmd(
            InputMode::Gate,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                tl.add_state(action::randomize_gates(tl, /*pattern,*/ min, max));
                return msuccess("Randomized Gate.");
            },
            ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}))),

    // Temporary --------------------------------------------------------------

    cmd("demo",
        [](XenTimeline &tl) {
            tl.set_aux_state({{0, {}}}, false); // Manually reset selection on overwrite
            tl.add_state(demo_state());
            return msuccess("Demo state loaded.");
        })

);

} // namespace xen
