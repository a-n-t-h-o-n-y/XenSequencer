#include <xen/gui/active_sessions.hpp>

#include <algorithm>

#include <xen/gui/color_ids.hpp>

namespace xen::gui
{

auto InstanceModel::add_item(juce::Uuid const &uuid, std::string const &name) -> void
{
    items_.emplace_back(uuid, name);
}

auto InstanceModel::add_or_update_item(juce::Uuid const &uuid, std::string const &name)
    -> void
{
    auto const it = std::find_if(std::begin(items_), std::end(items_),
                                 [&](auto const &item) { return item.first == uuid; });

    if (it != std::end(items_))
    {
        it->second = name;
    }
    else
    {
        items_.emplace_back(uuid, name);
    }
}

auto InstanceModel::remove_item(juce::Uuid const &uuid) -> void
{
    auto const it = std::find_if(std::cbegin(items_), std::cend(items_),
                                 [&](auto const &item) { return item.first == uuid; });

    if (it != std::cend(items_))
    {
        items_.erase(it);
    }
}

auto InstanceModel::getNumRows() -> int
{
    return static_cast<int>(items_.size());
}

auto InstanceModel::paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                                     bool rowIsSelected) -> void
{
    if (row >= 0 && row < this->getNumRows())
    {
        auto &laf = parent_.getLookAndFeel();
        if (rowIsSelected)
        {
            g.fillAll(
                laf.findColour((int)ActiveSessionsColorIDs::SelectedItemBackground));
            g.setColour(laf.findColour((int)ActiveSessionsColorIDs::SelectedItemText));
        }
        else
        {
            g.fillAll(laf.findColour((int)ActiveSessionsColorIDs::ItemBackground));
            g.setColour(laf.findColour((int)ActiveSessionsColorIDs::ItemText));
        }

        g.drawText(items_[(std::size_t)row].second, 2, 0, width - 4, height,
                   juce::Justification::centredLeft, true);
    }
}

auto InstanceModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent &) -> void
{
    if (row >= 0 && row < this->getNumRows())
    {
        on_instance_selected(items_[(std::size_t)row].first);
    }
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

NameEdit::NameEdit()
{
    // Makes the label editable on a double click
    this->setEditable(false, true, false);
}

auto NameEdit::textWasEdited() -> void
{
    on_name_changed(this->getText().toStdString());
}

auto NameEdit::lookAndFeelChanged() -> void
{
    this->setColour(juce::Label::textColourId,
                    this->findColour((int)ActiveSessionsColorIDs::CurrentItemText));
    this->setColour(
        juce::Label::backgroundColourId,
        this->findColour((int)ActiveSessionsColorIDs::CurrentItemBackground));
    this->setColour(juce::Label::outlineWhenEditingColourId,
                    this->findColour((int)ActiveSessionsColorIDs::OutlineWhenEditing));
    this->setColour(
        juce::Label::backgroundWhenEditingColourId,
        this->findColour((int)ActiveSessionsColorIDs::BackgroundWhenEditing));
    this->setColour(juce::Label::textWhenEditingColourId,
                    this->findColour((int)ActiveSessionsColorIDs::TextWhenEditing));
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

ActiveSessions::ActiveSessions()
{
    name_edit_.setText("", juce::dontSendNotification);
    this->addAndMakeVisible(name_edit_);

    instance_list_box_.setModel(&instance_model_);
    instance_list_box_.setRowHeight(20);
    this->addAndMakeVisible(instance_list_box_);

    // instance_list_box_.setWantsKeyboardFocus(false);
    // name_edit_.setWantsKeyboardFocus(false);
}

auto ActiveSessions::update_this_instance_name(std::string const &name) -> void
{
    name_edit_.setText(name, juce::dontSendNotification);
}

auto ActiveSessions::add_or_update_instance(juce::Uuid const &uuid,
                                            std::string const &name) -> void
{
    instance_model_.add_or_update_item(uuid, name);
    instance_list_box_.updateContent();
    instance_list_box_.repaint(); // Shouldn't be needed but is
}

auto ActiveSessions::remove_instance(juce::Uuid const &uuid) -> void
{
    instance_model_.remove_item(uuid);
    instance_list_box_.updateContent();
}

auto ActiveSessions::resized() -> void
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem{name_edit_}.withHeight(20.f));
    flexbox.items.add(juce::FlexItem{instance_list_box_}.withFlex(1.0f));

    flexbox.performLayout(this->getLocalBounds());
}

auto ActiveSessions::lookAndFeelChanged() -> void
{
    instance_list_box_.setColour(
        juce::ListBox::backgroundColourId,
        this->findColour((int)ActiveSessionsColorIDs::ItemBackground));
}

} // namespace xen::gui