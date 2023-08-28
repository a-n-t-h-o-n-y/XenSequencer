#include "xen_command_core.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sequence/sequence.hpp>

#include "actions.hpp"
#include "parse_args.hpp"
#include "selection.hpp"
#include "xen_timeline.hpp"

namespace xen
{

XenCommandCore::XenCommandCore(XenTimeline &t,
                               std::optional<sequence::Cell> &copy_buffer)
    : CommandCore{t}, copy_buffer_{copy_buffer}
{
    this->add_command({"undo", "undo", "Undo the last command.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           return tl.undo() ? "Undo Successful" : "Can't Undo ";
                       }});
    this->add_command({"redo", "redo", "Redo the last command.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           return tl.redo() ? "Redo Successful" : "Can't Redo";
                       }});

    this->add_command(
        {"demo", "demo", "Overwrite current state with demo state.",
         [](XenTimeline &tl, std::vector<std::string> const &) {
             tl.set_aux_state({{0, {}}}); // Manually reset selection on overwrite
             tl.add_state(demo_state());
             return "Demo state loaded.";
         }});

    this->add_command({"moveleft", "moveleft", "Move the selection left.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.set_aux_state(action::move_left(tl));
                           return "Moved left.";
                       }});

    this->add_command({"moveright", "moveright", "Move the selection right.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.set_aux_state(action::move_right(tl));
                           return "Moved right.";
                       }});

    this->add_command({"moveup", "moveup", "Move the selection up.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.set_aux_state(action::move_up(tl));
                           // TODO message depending on if moved or hit ceiling
                           return "Moved up.";
                       }});

    this->add_command({"movedown", "movedown", "Move the selection down.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.set_aux_state(action::move_down(tl));
                           return "Moved down.";
                       }});

    this->add_command({"copy", "copy", "Copy the current selection.",
                       [this](XenTimeline &tl, std::vector<std::string> const &) {
                           copy_buffer_ = action::copy(tl);
                           return "Copied.";
                       }});

    this->add_command({"cut", "cut", "Cut the current selection.",
                       [this](XenTimeline &tl, std::vector<std::string> const &) {
                           auto [buffer, state] = action::cut(tl);
                           tl.add_state(std::move(state));
                           copy_buffer_ = buffer;
                           return "Cut.";
                       }});

    this->add_command({"paste", "paste",
                       "Paste the copied Cell to the current selection.",
                       [this](XenTimeline &tl, std::vector<std::string> const &) {
                           auto const result = action::paste(tl, copy_buffer_);
                           if (result.has_value())
                           {
                               tl.add_state(*result);
                               return "Pasted.";
                           }
                           return "Nothing to paste.";
                       }});

    this->add_command({"duplicate", "duplicate",
                       "Duplicate the current selection to the right.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           auto [aux, state] = action::duplicate(tl);
                           tl.set_aux_state(std::move(aux));
                           tl.add_state(std::move(state));
                           return "Duplicated.";
                       }});

    this->add_command({"mode", "mode [name]", "Change the current input mode.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [mode_str] = extract_args<std::string>(args);
                           auto const mode = parse_input_mode(mode_str);
                           tl.set_aux_state(action::set_mode(tl, mode));
                           return "Changed mode to '" + args[0] + "'.";
                       }});

    this->add_command({"note", "note [interval=0] [velocity=0.8] [delay=0] [gate=1]",
                       "Change the current Cell to a Note.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           // TODO specify all args
                           auto const [interval] = extract_args<int>(args);
                           tl.add_state(action::note(tl, interval, 0.8f, 0.f, 1.f));
                           return "Added note.";
                       }});

    this->add_command({"rest", "rest", "Change the current Cell to a Rest.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.add_state(action::rest(tl));
                           return "Added rest.";
                       }});

    this->add_command({"flip", "flip", "Flip the current Cell between Cell types.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           tl.add_state(action::flip(tl));
                           return "Flipped.";
                       }});

    this->add_command({"split", "split [count=2]", "Split the current Cell.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [count] = extract_args<std::size_t>(args);
                           tl.add_state(action::split(tl, count));
                           return "Split.";
                       }});

    this->add_command({"extract", "extract", "Replace parent with selected child.",
                       [](XenTimeline &tl, std::vector<std::string> const &) {
                           auto [state, aux] = action::extract(tl);
                           tl.set_aux_state(aux);
                           tl.add_state(state);
                           return "Extracted.";
                       }});

    this->add_command({"shiftnote", "shiftNote [amount]",
                       "Shift the current Note by a number of intervals.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<int>(args);
                           tl.add_state(action::shift_note(tl, amount));
                           return "Shifted.";
                       }});

    this->add_command({"shiftoctave", "shiftOctave [amount]",
                       "Shift the current Note's octave.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<int>(args);
                           tl.add_state(action::shift_note_octave(tl, amount));
                           return "Shifted.";
                       }});

    this->add_command({"shiftvelocity", "shiftVelocity [amount]",
                       "Shift the current Note's velocity.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::shift_velocity(tl, amount));
                           return "Shifted.";
                       }});

    this->add_command({"shiftdelay", "shiftDelay [amount]",
                       "Shift the current Note's delay.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::shift_delay(tl, amount));
                           return "Shifted.";
                       }});

    this->add_command({"shiftgate", "shiftGate [amount]",
                       "Shift the current Note's gate.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::shift_gate(tl, amount));
                           return "Shifted.";
                       }});

    this->add_command({"setnote", "setNote [interval]",
                       "Set a Note's interval to the specified amount.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [interval] = extract_args<int>(args);
                           tl.add_state(action::set_note(tl, interval));
                           return "Set.";
                       }});

    this->add_command({"setoctave", "setOctave [amount]",
                       "Set a Note's octave to the specified amount.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<int>(args);
                           tl.add_state(action::set_note_octave(tl, amount));
                           return "Set.";
                       }});

    this->add_command({"setvelocity", "setVelocity [amount]",
                       "Set a Note's velocity to the specified amount.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::set_velocity(tl, amount));
                           return "Set.";
                       }});

    this->add_command({"setdelay", "setDelay [amount]",
                       "Set a Note's delay to the specified amount.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::set_delay(tl, amount));
                           return "Set.";
                       }});

    this->add_command({"setgate", "setGate [amount]",
                       "Set a Note's gate to the specified amount.",
                       [](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [amount] = extract_args<float>(args);
                           tl.add_state(action::set_gate(tl, amount));
                           return "Set.";
                       }});

    this->add_command({"focus", "focus [name]", "Focus on a specific Phrase.",
                       [this](XenTimeline &tl, std::vector<std::string> const &args) {
                           auto const [name] = extract_args<std::string>(args);
                           this->on_focus_change_request(name);
                           return "Focused on '" + name + "'.";
                       }});

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