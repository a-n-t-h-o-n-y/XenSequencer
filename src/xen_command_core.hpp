#pragma once

#include <variant>

#include <sequence/modify.hpp>

#include "command_core.hpp"
#include "selection.hpp"
#include "xen_timeline.hpp"

namespace xen
{

/**
 * @brief The CommandCore with all Xen-specific commands.
 */
class XenCommandCore : public CommandCore
{
  public:
    explicit XenCommandCore(XenTimeline &t) : CommandCore{t}
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
                 tl.add_state(demo_state());
                 tl.set_aux_state({0, {}}); // Manually reset selection on overwrite
                 return "Demo state loaded.";
             }});

        this->add_command({"moveleft", "moveleft", "Move the selection left.",
                           [](XenTimeline &tl, std::vector<std::string> const &) {
                               auto aux = tl.get_aux_state();
                               auto const phrase = tl.get_state().first.phrase;
                               aux.selected = move_left(phrase, aux.selected);
                               tl.set_aux_state(aux);
                               return "Moved left.";
                           }});

        this->add_command({"moveright", "moveright", "Move the selection right.",
                           [](XenTimeline &tl, std::vector<std::string> const &) {
                               auto aux = tl.get_aux_state();
                               auto const phrase = tl.get_state().first.phrase;
                               aux.selected = move_right(phrase, aux.selected);
                               tl.set_aux_state(aux);
                               return "Moved right.";
                           }});

        this->add_command({"moveup", "moveup", "Move the selection up.",
                           [](XenTimeline &tl, std::vector<std::string> const &) {
                               auto aux = tl.get_aux_state();
                               aux.selected = move_up(aux.selected);
                               tl.set_aux_state(aux);
                               // TODO message depending on if moved or hit ceiling
                               return "Moved up.";
                           }});

        this->add_command({"movedown", "movedown", "Move the selection down.",
                           [](XenTimeline &tl, std::vector<std::string> const &) {
                               auto aux = tl.get_aux_state();
                               auto const phrase = tl.get_state().first.phrase;
                               aux.selected = move_down(phrase, aux.selected);
                               tl.set_aux_state(aux);
                               return "Moved down.";
                           }});

        this->add_command(
            {"randomizetest", "randomizetest", "Randomize the current sequence.",
             [](XenTimeline &tl, std::vector<std::string> const &) {
                 auto const aux = tl.get_aux_state();
                 auto [state, _] = tl.get_state();
                 sequence::Cell &cell = get_selected_cell(state.phrase, aux.selected);
                 cell = sequence::modify::randomize_intervals(cell, -12, 12);
                 tl.add_state(state);
                 return "Randomized.";
             }});
    }
};

} // namespace xen