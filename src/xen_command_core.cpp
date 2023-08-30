#include "xen_command_core.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sequence/sequence.hpp>

#include "actions.hpp"
#include "command.hpp"
#include "parse_args.hpp"
#include "selection.hpp"
#include "xen_timeline.hpp"

namespace xen
{

XenCommandCore::XenCommandCore(XenTimeline &t,
                               std::optional<sequence::Cell> &copy_buffer)
    : CommandCore{t}, copy_buffer_{copy_buffer}
{
    add(cmd("undo", "Undo the last command.", [](XenTimeline &tl) {
        return tl.undo() ? "Undo Successful" : "Can't Undo ";
    }));

    add(cmd("redo", "Redo the last command.", [](XenTimeline &tl) {
        return tl.redo() ? "Redo Successful" : "Can't Redo";
    }));

    add(cmd("moveLeft", "Move the selection left.", [](XenTimeline &tl) {
        tl.set_aux_state(action::move_left(tl));
        return "Moved left.";
    }));

    add(cmd("moveRight", "Move the selection right.", [](XenTimeline &tl) {
        tl.set_aux_state(action::move_right(tl));
        return "Moved right.";
    }));

    add(cmd("moveUp", "Move the selection up.", [](XenTimeline &tl) {
        tl.set_aux_state(action::move_up(tl));
        // TODO message depending on if moved or hit ceiling
        return "Moved up.";
    }));

    add(cmd("moveDown", "Move the selection down.", [](XenTimeline &tl) {
        tl.set_aux_state(action::move_down(tl));
        // TODO message depending on if moved or hit floor
        return "Moved down.";
    }));

    add(cmd("copy", "Copy the current selection.", [this](XenTimeline &tl) {
        copy_buffer_ = action::copy(tl);
        return "Copied.";
    }));

    add(cmd("cut", "Cut the current selection.", [this](XenTimeline &tl) {
        auto [buffer, state] = action::cut(tl);
        tl.add_state(std::move(state));
        copy_buffer_ = buffer;
        return "Cut.";
    }));

    add(cmd("paste", "Paste the copied Cell to the current selection.",
            [this](XenTimeline &tl) {
                auto const result = action::paste(tl, copy_buffer_);
                if (result.has_value())
                {
                    tl.add_state(*result);
                    return "Pasted.";
                }
                return "Nothing to paste.";
            }));

    add(cmd("duplicate", "Duplicate the current selection to the right.",
            [](XenTimeline &tl) {
                auto [aux, state] = action::duplicate(tl);
                tl.set_aux_state(std::move(aux));
                tl.add_state(std::move(state));
                return "Duplicated.";
            }));

    add(cmd(
        "mode", "Change the current input mode.",
        [](XenTimeline &tl, std::string const &mode_str) {
            auto const mode = parse_input_mode(mode_str);
            tl.set_aux_state(action::set_mode(tl, mode));
            return "Changed mode to '" + mode_str + "'.";
        },
        ArgInfo<std::string>{"mode"}));

    add(cmd(
        "note", "Change the current Cell to a Note.",
        [](XenTimeline &tl, int interval, float velocity, float delay, float gate) {
            tl.add_state(action::note(tl, interval, velocity, delay, gate));
            return "Added note.";
        },
        ArgInfo<int>{"interval", 0}, ArgInfo<float>{"velocity", 0.8f},
        ArgInfo<float>{"delay", 0.f}, ArgInfo<float>{"gate", 1.f}));

    add(cmd("rest", "Change the current Cell to a Rest.", [](XenTimeline &tl) {
        tl.add_state(action::rest(tl));
        return "Added rest.";
    }));

    add(cmd("flip", "Flip the current Cell between Cell types.", [](XenTimeline &tl) {
        tl.add_state(action::flip(tl));
        return "Flipped.";
    }));

    add(cmd(
        "split", "Split the current Cell.",
        [](XenTimeline &tl, std::size_t count) {
            tl.add_state(action::split(tl, count));
            return "Split.";
        },
        ArgInfo<std::size_t>{"count", 2}));

    add(cmd("extract", "Extract the current Cell.", [](XenTimeline &tl) {
        auto [state, aux] = action::extract(tl);
        tl.set_aux_state(aux);
        tl.add_state(state);
        return "Extracted.";
    }));

    add(cmd(
        "shiftNote", "Shift the current Note by a number of intervals.",
        [](XenTimeline &tl, int amount) {
            tl.add_state(action::shift_note(tl, amount));
            return "Shifted.";
        },
        ArgInfo<int>{"amount", 1}));

    add(cmd(
        "shiftOctave", "Shift the current Note's octave.",
        [](XenTimeline &tl, int amount) {
            tl.add_state(action::shift_note_octave(tl, amount));
            return "Shifted.";
        },
        ArgInfo<int>{"amount", 1}));

    add(cmd(
        "shiftVelocity", "Shift the current Note's velocity.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::shift_velocity(tl, amount));
            return "Shifted.";
        },
        ArgInfo<float>{"amount", 0.1f}));

    add(cmd(
        "shiftDelay", "Shift the current Note's delay.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::shift_delay(tl, amount));
            return "Shifted.";
        },
        ArgInfo<float>{"amount", 0.1f}));

    add(cmd(
        "shiftGate", "Shift the current Note's gate.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::shift_gate(tl, amount));
            return "Shifted.";
        },
        ArgInfo<float>{"amount", 0.1f}));

    add(cmd(
        "setNote", "Set the current Note's interval.",
        [](XenTimeline &tl, int interval) {
            tl.add_state(action::set_note(tl, interval));
            return "Set.";
        },
        ArgInfo<int>{"interval", 0}));

    add(cmd(
        "setOctave", "Set the current Note's octave.",
        [](XenTimeline &tl, int amount) {
            tl.add_state(action::set_note_octave(tl, amount));
            return "Set.";
        },
        ArgInfo<int>{"amount", 0}));

    add(cmd(
        "setVelocity", "Set the current Note's velocity.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::set_velocity(tl, amount));
            return "Set.";
        },
        ArgInfo<float>{"amount", 0.8f}));

    add(cmd(
        "setDelay", "Set the current Note's delay.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::set_delay(tl, amount));
            return "Set.";
        },
        ArgInfo<float>{"amount", 0.f}));

    add(cmd(
        "setGate", "Set the current Note's gate.",
        [](XenTimeline &tl, float amount) {
            tl.add_state(action::set_gate(tl, amount));
            return "Set.";
        },
        ArgInfo<float>{"amount", 1.f}));

    this->add(cmd(
        "focus", "Focus on a specific Phrase.",
        [this](auto &, std::string const &name) {
            this->on_focus_change_request(name);
            return "Focused on '" + name + "'.";
        },
        ArgInfo<std::string>{"component"}));

    add(cmd("demo", "Overwrite current state with demo state.", [](XenTimeline &tl) {
        tl.set_aux_state({{0, {}}}); // Manually reset selection on overwrite
        tl.add_state(demo_state());
        return "Demo state loaded.";
    }));

    // this->add_command(
    //     {"randomizetest", "randomizetest", "Randomize the current sequence.",
    //      [](XenTimeline &tl, std::vector<std::string> const &) {
    //          auto const aux = tl.get_aux_state();
    //          auto [state, _] = tl.get_state();
    //          sequence::Cell &cell = get_selected_cell(state.phrase,
    //          aux.selected); cell = sequence::modify::randomize_intervals(cell,
    //          -12, 12); tl.add_state(state); return "Randomized.";
    //      }});
}

} // namespace xen