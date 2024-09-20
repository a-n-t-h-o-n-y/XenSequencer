#include <xen/gui/active_sessions.hpp>

#include <algorithm>

#include <xen/gui/themes.hpp>

namespace xen::gui
{

InstanceModel::InstanceModel(juce::Component &parent) : parent_{parent}
{
}

void InstanceModel::add_item(juce::Uuid const &uuid, std::string const &name)
{
    items_.emplace_back(uuid, name);
}

void InstanceModel::add_or_update_item(juce::Uuid const &uuid, std::string const &name)
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

void InstanceModel::remove_item(juce::Uuid const &uuid)
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

void InstanceModel::paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                                     bool row_is_selected)
{
    if (row >= 0 && row < this->getNumRows())
    {
        auto &laf = parent_.getLookAndFeel();
        if (row_is_selected)
        {
            g.fillAll(laf.findColour(ColorID::BackgroundLow));
            g.setColour(laf.findColour(ColorID::ForegroundHigh));
        }
        else
        {
            g.fillAll(laf.findColour(ColorID::BackgroundMedium));
            g.setColour(laf.findColour(ColorID::ForegroundHigh));
        }

        g.drawText(items_[(std::size_t)row].second, 2, 0, width - 4, height,
                   juce::Justification::centredLeft, true);
    }
}

void InstanceModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent &)
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

void NameEdit::textWasEdited()
{
    on_name_changed(this->getText().toStdString());
}

void NameEdit::lookAndFeelChanged()
{
    this->setColour(juce::Label::textColourId,
                    this->findColour(ColorID::ForegroundMedium));
    this->setColour(juce::Label::backgroundColourId,
                    this->findColour(ColorID::BackgroundMedium));
    this->setColour(juce::Label::outlineWhenEditingColourId,
                    this->findColour(ColorID::ForegroundLow));
    this->setColour(juce::Label::backgroundWhenEditingColourId,
                    this->findColour(ColorID::BackgroundLow));
    this->setColour(juce::Label::textWhenEditingColourId,
                    this->findColour(ColorID::ForegroundHigh));
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

ActiveSessionsList::ActiveSessionsList()
{
    this->setComponentID("ActiveSessionsList");

    name_edit_.setText("", juce::dontSendNotification);
    this->addAndMakeVisible(name_edit_);

    instance_list_box_.setModel(&instance_model_);
    instance_list_box_.setRowHeight(20);
    this->addAndMakeVisible(instance_list_box_);
}

void ActiveSessionsList::update_this_instance_name(std::string const &name)
{
    name_edit_.setText(name, juce::dontSendNotification);
}

void ActiveSessionsList::add_or_update_instance(juce::Uuid const &uuid,
                                                std::string const &name)
{
    instance_model_.add_or_update_item(uuid, name);
    instance_list_box_.updateContent();
    instance_list_box_.repaint(); // Shouldn't be needed but is
}

void ActiveSessionsList::remove_instance(juce::Uuid const &uuid)
{
    instance_model_.remove_item(uuid);
    instance_list_box_.updateContent();
}

void ActiveSessionsList::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem{name_edit_}.withHeight(20.f));
    flexbox.items.add(juce::FlexItem{instance_list_box_}.withFlex(1.0f));

    flexbox.performLayout(this->getLocalBounds());
}

void ActiveSessionsList::lookAndFeelChanged()
{
    instance_list_box_.setColour(juce::ListBox::backgroundColourId,
                                 this->findColour(ColorID::BackgroundMedium));
}

} // namespace xen::gui