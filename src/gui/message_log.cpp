#include <xen/gui/message_log.hpp>

#include <cassert>
#include <cstddef>
#include <sstream>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/xen_list_box.hpp>
#include <xen/message_level.hpp>
namespace xen::gui
{

MessageLog::MessageLog() : XenListBox{"MessageLog"}
{
}

void MessageLog::add_message(juce::String const &text, ::xen::MessageLevel level)
{
    auto oss = std::ostringstream{};
    oss << level;
    auto const level_str = juce::String{oss.str()};

    auto const max_level_length = 7;
    assert(level_str.length() <= max_level_length);

    auto const time_str = juce::Time::getCurrentTime().toString(false, true, true);

    messages_.emplace_back(
        time_str + " | " + level_str +
        std::string((std::size_t)(max_level_length - level_str.length()), ' ') + " | " +
        text);

    this->updateContent();
}

auto MessageLog::getNumRows() -> int
{
    return static_cast<int>(messages_.size());
}

auto MessageLog::get_row_display(std::size_t index) -> juce::String
{
    return (index < messages_.size()) ? messages_[index] : "";
}

void MessageLog::item_selected(std::size_t index)
{
    // Do nothing.
}

} // namespace xen::gui