#include "xen_command_core.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

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
        auto copy = action::copy(tl);
        if (copy.has_value())
        {
            copy_buffer_ = std::move(*copy);
            return "Copied.";
        }
        return "Nothing to copy.";
    }));

    add(cmd("cut", "Cut the current selection.", [this](XenTimeline &tl) {
        auto cut = action::cut(tl);
        if (!cut.has_value())
        {
            return "Nothing to cut.";
        }
        auto &[buffer, state] = *cut;
        tl.add_state(std::move(state));
        copy_buffer_ = std::move(buffer);
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
                tl.set_aux_state(std::move(aux), false);
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
        tl.set_aux_state(aux, false);
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

    add(cmd(
        "focus", "Focus on a specific Phrase.",
        [this](auto &, std::string const &name) {
            this->on_focus_change_request(name);
            return "Focused on '" + name + "'.";
        },
        ArgInfo<std::string>{"component"}));

    add(cmd(
        "addMeasure", "Add a measure to the end of the Phrase.",
        [](XenTimeline &tl, sequence::TimeSignature const &ts) {
            auto [aux, state] = action::add_measure(tl, ts);
            tl.set_aux_state(std::move(aux), false);
            tl.add_state(std::move(state));
            return "Added measure.";
        },
        ArgInfo<sequence::TimeSignature>("duration", {{4, 4}})));

    add(cmd("delete", "Delete the current Cell or Measure.", [](XenTimeline &tl) {
        auto [aux, state] =
            action::delete_cell(tl.get_aux_state(), tl.get_state().first);
        tl.set_aux_state(aux, false);
        tl.add_state(state);
        return "Deleted.";
    }));

    add(cmd(
        "save", "Save the current state to a file.",
        [](XenTimeline &tl, std::string const &filepath) {
            action::save_state(tl, filepath);
            return "Saved to '" + filepath + "'.";
        },
        ArgInfo<std::string>{"filepath"}));

    add(cmd(
        "load", "Load State from a file.",
        [](XenTimeline &tl, std::string const &filepath) {
            tl.set_aux_state({{0, {}}}, false); // Manually reset selection on overwrite
            tl.add_state(action::load_state(filepath));
            return "Loaded from '" + filepath + "'.";
        },
        ArgInfo<std::string>{"filepath"}));

    // Temporary ----------------------------------------------------------------

    add(cmd("demo", "Overwrite current state with demo state.", [](XenTimeline &tl) {
        tl.set_aux_state({{0, {}}}, false); // Manually reset selection on overwrite
        tl.add_state(demo_state());
        return "Demo state loaded.";
    }));
}

} // namespace xen