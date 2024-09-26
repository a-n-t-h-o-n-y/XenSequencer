#include <xen/gui/message_log.hpp>

#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/message_level.hpp>

namespace xen::gui
{

void MessageLog::Log::append(juce::String const &text, juce::Colour color)
{
    content_.append("\n" + juce::Time::getCurrentTime().toString(false, true, true) +
                        " :: " + text,
                    fonts::monospaced().regular.withHeight(18.f), color);
    this->refresh();
}

void MessageLog::Log::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundHigh));
    layout_.draw(g, this->getLocalBounds().toFloat());
}

void MessageLog::Log::resized()
{
    this->refresh();
}

void MessageLog::Log::refresh()
{
    layout_.createLayout(content_, (float)this->getWidth());
    this->setSize(this->getWidth(), (int)std::ceil(layout_.getHeight()));
    this->repaint();
}

// -------------------------------------------------------------------------------------

MessageLog::MessageLog()
{
    this->setViewedComponent(&log_, false);
    this->setComponentID("MessageLog");
    this->setWantsKeyboardFocus(true);
}

void MessageLog::add_message(juce::String const &text, ::xen::MessageLevel level)
{
    // TODO On lookAndFeelUpdated you are not going to be able to change colors.
    // You'd have to implement that with attribute string somehow.

    log_.append(text, this->findColour(get_color_id(level)));
    this->getVerticalScrollBar().setRangeLimits(0, log_.getHeight());
    this->setViewPosition(0, log_.getHeight() - this->getHeight());
}

void MessageLog::resized()
{
    log_.setSize(this->getLocalBounds().getWidth(), 0);
}

} // namespace xen::gui