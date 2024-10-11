#pragma once

#include <string>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/xen_list_box.hpp>

namespace xen::gui
{

/**
 * Stores and Displays Active Session MetaData.
 */
class SessionsListBox : public XenListBox
{
  public:
    sl::Signal<void(juce::Uuid const &)> on_session_selected;

  public:
    SessionsListBox();

  public:
    void add_item(juce::Uuid const &uuid, juce::String const &name);

    /**
     * Add or update an item's display_name.
     * @details If the item is not found by the given UUID, it is added.
     * @param uuid The UUID of the instance.
     * @param name The new display name of the instance.
     */
    void add_or_update_item(juce::Uuid const &uuid, juce::String const &name);

    /**
     * @details Does nothing if ]p uuid is not found.
     */
    void remove_item(juce::Uuid const &uuid);

  protected:
    auto get_row_display(std::size_t index) -> juce::String override;

    void item_selected(std::size_t index) override;

    auto getNumRows() -> int override;

  private:
    struct MetaData
    {
        juce::Uuid uuid;
        juce::String display_name;
    };
    std::vector<MetaData> items_;
};

// -------------------------------------------------------------------------------------

/**
 * A label that can be edited.
 */
class NameEdit : public juce::Label
{
  public:
    sl::Signal<void(juce::String const &)> on_name_changed;

  public:
    NameEdit();

  public:
    void set_name(juce::String const &name);

  protected:
    void textWasEdited() override;

    void lookAndFeelChanged() override;
};

// -------------------------------------------------------------------------------------

/**
 * Lists active session and current session names. Does not perform any logic, only
 * emits signals.
 */
class ActiveSessionsList : public juce::Component
{
  public:
    NameEdit current_session_name_edit;
    SessionsListBox sessions_list_box;

  public:
    ActiveSessionsList();

  public:
    void resized() override;
};

} // namespace xen::gui