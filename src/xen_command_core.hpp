#pragma once

#include "command_core.hpp"
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

        this->add_command({"demo", "demo", "Overwrite current state with demo state.",
                           [](XenTimeline &tl, std::vector<std::string> const &) {
                               tl.add_state(demo_state());
                               return "Demo state loaded.";
                           }});
    }
};

} // namespace xen