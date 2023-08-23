#pragma once

#include <optional>
#include <utility>
#include <variant>

#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>

#include "actions.hpp"
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
    explicit XenCommandCore(XenTimeline &t, std::optional<sequence::Cell> &copy_buffer)
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
                 tl.add_state(demo_state());
                 tl.set_aux_state({0, {}}); // Manually reset selection on overwrite
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

  private:
    std::optional<sequence::Cell> &copy_buffer_;
};

} // namespace xen