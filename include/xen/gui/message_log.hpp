#pragma once

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

class MessageLog : public XenListBox
{
  public:
    MessageLog();

  public:
    void add_message(juce::String const &text, ::xen::MessageLevel level);

    /**
     * Gets the total number of rows (messages) in the log.
     * @return The total number of rows.
     */
    [[nodiscard]] auto getNumRows() -> int override;

    /**
     * Returns the string to display for a given row.
     * @param index The row index to display.
     * @return The message to display at the specified row.
     */
    [[nodiscard]] auto get_row_display(std::size_t index) -> juce::String override;

    void item_selected(std::size_t index) override;

  private:
    std::vector<juce::String> messages_;
};

} // namespace xen::gui