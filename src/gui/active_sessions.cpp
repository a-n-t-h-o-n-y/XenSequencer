#include <xen/gui/active_sessions.hpp>

#include <algorithm>

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
        if (rowIsSelected)
        {
            g.fillAll(juce::Colours::lightblue);
        }

        g.setColour(juce::Colours::black);
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

ActiveSessions::ActiveSessions()
{
    label_.setText("Active Sessions", juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::Font(15.0f, juce::Font::bold));
    this->addAndMakeVisible(label_);

    instance_list_box_.setModel(&instance_model_);
    instance_list_box_.setRowHeight(20);
    this->addAndMakeVisible(instance_list_box_);
}

auto ActiveSessions::add_or_update_instance(juce::Uuid const &uuid,
                                            std::string const &name) -> void
{
    instance_model_.add_or_update_item(uuid, name);
    instance_list_box_.updateContent();
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

    flexbox.items.add(juce::FlexItem{label_}.withHeight(20.0f));
    flexbox.items.add(juce::FlexItem{instance_list_box_}.withFlex(1.0f));

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen::gui