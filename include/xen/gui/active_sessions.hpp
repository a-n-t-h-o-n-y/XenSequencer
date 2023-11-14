#pragma once

#include <string>
#include <utility>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

class InstanceModel : public juce::ListBoxModel
{
  public:
    sl::Signal<void(juce::Uuid const &)> on_instance_selected;

  public:
    /**
     * @brief Add an item to the listbox.
     *
     * @param uuid The UUID of the instance.
     * @param item The display name of the instance.
     */
    auto add_item(juce::Uuid const &uuid, std::string const &name) -> void;

    /**
     * @brief Add or update an item's name in the listbox.
     *
     * If the item is not found by the given UUID, it is added.
     *
     * @param uuid The UUID of the instance.
     * @param name The new display name of the instance.
     */
    auto add_or_update_item(juce::Uuid const &uuid, std::string const &name) -> void;

    /**
     * @brief Remove an item from the listbox.
     *
     * @param uuid The UUID of the instance to remove.
     */
    auto remove_item(juce::Uuid const &uuid) -> void;

  public:
    [[nodiscard]] auto getNumRows() -> int override;

    auto paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) -> void override;

    auto listBoxItemDoubleClicked(int row, const juce::MouseEvent &) -> void override;

  private:
    // Instance UUID, Display Name
    std::vector<std::pair<juce::Uuid, std::string>> items_;
};

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

class ActiveSessions : public juce::Component
{
  public:
    ActiveSessions();

  public:
    /**
     * @brief Add or update an instance in the listbox.
     *
     * @param uuid The UUID of the instance.
     * @param name The display name of the instance.
     */
    auto add_or_update_instance(juce::Uuid const &uuid, std::string const &name)
        -> void;

    /**
     * @brief Remove an instance from the listbox.
     *
     * @param uuid The UUID of the instance to remove.
     */
    auto remove_instance(juce::Uuid const &uuid) -> void;

  protected:
    auto resized() -> void override;

  private:
    juce::Label label_;
    juce::ListBox instance_list_box_;
    InstanceModel instance_model_;

  public:
    sl::Signal<void(juce::Uuid const &)> &on_instance_selected{
        instance_model_.on_instance_selected};

    sl::Signal<void(std::string const &)> on_this_instance_name_change;
};

} // namespace xen::gui