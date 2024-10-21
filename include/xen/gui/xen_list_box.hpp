#pragma once

#include <cstddef>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * A custom ListBox to display string rows.
 * @details Override the ListBoxModel::getNumRows, get_row_display, and item_selected
 * virtual functions.
 */
class XenListBox : public juce::ListBoxModel, public juce::ListBox
{
  public:
    explicit XenListBox(juce::String const &component_id);

  public:
    /**
     * Return the string to be displayed for the given row.
     * @details Font and Colors are hardcoded in the base class.
     */
    [[nodiscard]] virtual auto get_row_display(std::size_t index) -> juce::String = 0;

    /**
     * Be notified when an item is selected by double click or enter key.
     */
    virtual void item_selected(std::size_t index) = 0;

  public:
    void listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse) override;

    void returnKeyPressed(int last_row_selected) override;

    auto keyPressed(juce::KeyPress const &key) -> bool override;

    void lookAndFeelChanged() override;

    void paintListBoxItem(int row_number, juce::Graphics &g, int width, int height,
                          bool row_is_selected) override;
};

} // namespace xen::gui