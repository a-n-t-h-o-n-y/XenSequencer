#include <xen/gui/message_log.hpp>

#include <memory>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/message_level.hpp>

namespace xen::gui
{

MessageLog::MessageLog()
{
    this->setViewedComponent(&log, false);
    this->setComponentID("MessageLog");
    this->setWantsKeyboardFocus(true);
}

void MessageLog::resized()
{
    log.setSize(this->getLocalBounds().getWidth(), 0);
}

MessageLog::Log::Entry::Entry(juce::String const &message, ::xen::MessageLevel level)
    : level_{level}
{
    this->setFont(fonts::monospaced().regular.withHeight(18.f));
    this->setText(message, juce::dontSendNotification);
}

void MessageLog::Log::Entry::lookAndFeelChanged()
{
    this->setColour(juce::Label::backgroundColourId,
                    this->findColour(ColorID::BackgroundHigh));
    this->setColour(juce::Label::textColourId, this->findColour(get_color_id(level_)));
}

void MessageLog::Log::add_message(juce::String const &message,
                                  ::xen::MessageLevel level)
{
    entries_.push_back(std::make_unique<Entry>(message, level));
    this->addAndMakeVisible(*entries_.back());
    entries_.back()->lookAndFeelChanged();
    this->resized();
    this->scroll_to_bottom();
}

void MessageLog::Log::resized()
{
    auto const area = this->getLocalBounds();
    auto const message_height = 23;

    int y = 0;
    for (auto &component : entries_)
    {
        component->setBounds(0, y, area.getWidth(), message_height);
        y += message_height;
    }

    this->setSize(this->getWidth(), y);
}

void MessageLog::Log::scroll_to_bottom()
{
    if (auto *parent_viewport = this->findParentComponentOfClass<juce::Viewport>())
        parent_viewport->setViewPosition(
            0, juce::jmax(0, this->getHeight() - parent_viewport->getHeight()));
}

} // namespace xen::gui