#pragma once

#include <string>
#include <utility>
#include <vector>

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
    InstanceModel(juce::Component &parent);

  public:
    /**
     * Add an item to the listbox.
     *
     * @param uuid The UUID of the instance.
     * @param item The display name of the instance.
     */
    void add_item(juce::Uuid const &uuid, std::string const &name);

    /**
     * Add or update an item's name in the listbox.
     *
     * @details If the item is not found by the given UUID, it is added.
     * @param uuid The UUID of the instance.
     * @param name The new display name of the instance.
     */
    void add_or_update_item(juce::Uuid const &uuid, std::string const &name);

    /**
     * Remove an item from the listbox.
     *
     * @param uuid The UUID of the instance to remove.
     */
    void remove_item(juce::Uuid const &uuid);

  public:
    [[nodiscard]] auto getNumRows() -> int override;

    void paintListBoxItem(int row_number, juce::Graphics &g, int width, int height,
                          bool row_is_selected) override;

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override;

  private:
    // {Instance UUID, Display Name}
    std::vector<std::pair<juce::Uuid, std::string>> items_;
    juce::Component &parent_;
};

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

/**
 * A label that can be edited.
 */
class NameEdit : public juce::Label
{
  public:
    sl::Signal<void(std::string const &)> on_name_changed;

  public:
    NameEdit();

  protected:
    void textWasEdited() override;

    void lookAndFeelChanged() override;
};

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

class ActiveSessionsList : public juce::Component
{
  public:
    ActiveSessionsList();

  public:
    /**
     * Update the name of 'this' instance.
     *
     * @param name The new name of this instance.
     */
    void update_this_instance_name(std::string const &name);

    /**
     * Add or update an instance in the listbox.
     *
     * @param uuid The UUID of the instance.
     * @param name The display name of the instance.
     */
    void add_or_update_instance(juce::Uuid const &uuid, std::string const &name);

    /**
     * Remove an instance from the listbox.
     *
     * @param uuid The UUID of the instance to remove.
     */
    void remove_instance(juce::Uuid const &uuid);

  public:
    void resized() override;

    void lookAndFeelChanged() override;

  private:
    NameEdit name_edit_;
    juce::ListBox instance_list_box_;
    InstanceModel instance_model_{*this};

  public:
    sl::Signal<void(juce::Uuid const &)> &on_instance_selected{
        instance_model_.on_instance_selected};

    sl::Signal<void(std::string const &)> &on_this_instance_name_change{
        name_edit_.on_name_changed};
};

} // namespace xen::gui