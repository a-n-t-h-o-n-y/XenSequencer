#include <xen/gui/active_sessions_view.hpp>

#include <algorithm>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

namespace xen::gui
{

SessionsListBox::SessionsListBox() : XenListBox{"SessionsListBox"}
{
}

void SessionsListBox::add_item(juce::Uuid const &uuid, juce::String const &name)
{
    items_.push_back({uuid, name}); // with emplace_back clang 14 complains
}

void SessionsListBox::add_or_update_item(juce::Uuid const &uuid,
                                         juce::String const &name)
{
    auto const it = std::find_if(std::begin(items_), std::end(items_),
                                 [&](auto const &item) { return item.uuid == uuid; });

    if (it != std::end(items_))
    {
        it->display_name = name;
    }
    else
    {
        this->add_item(uuid, name);
    }

    this->updateContent();
    // TODO can you remove the below?
    this->repaint(); // Shouldn't be needed but is
}

void SessionsListBox::remove_item(juce::Uuid const &uuid)
{
    auto const it = std::find_if(std::cbegin(items_), std::cend(items_),
                                 [&](auto const &item) { return item.uuid == uuid; });

    if (it != std::cend(items_))
    {
        items_.erase(it);
    }

    this->updateContent();
}

auto SessionsListBox::get_row_display(std::size_t index) -> juce::String
{
    return items_[index].display_name;
}

void SessionsListBox::item_selected(std::size_t index)
{
    this->on_session_selected(items_[index].uuid);
}

auto SessionsListBox::getNumRows() -> int
{
    return (int)items_.size();
}

// -------------------------------------------------------------------------------------

NameEdit::NameEdit()
{
    // Makes the label editable on a double click
    this->setEditable(false, true, false);
    this->setFont(fonts::monospaced().regular.withHeight(17.f));
    this->setText("", juce::dontSendNotification);
}

void NameEdit::set_name(juce::String const &name)
{
    this->setText(name, juce::dontSendNotification);
}

void NameEdit::textWasEdited()
{
    this->on_name_changed(this->getText());
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

// -------------------------------------------------------------------------------------

ActiveSessionsView::ActiveSessionsView()
{
    this->addAndMakeVisible(current_session_name_edit);
    this->addAndMakeVisible(sessions_list_box);
}

void ActiveSessionsView::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem{current_session_name_edit}.withHeight(20.f));
    flexbox.items.add(juce::FlexItem{sessions_list_box}.withFlex(1.0f));

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen::gui