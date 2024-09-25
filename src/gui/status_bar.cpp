#include <xen/gui/status_bar.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/message_level.hpp>

namespace xen::gui
{

StatusBar::StatusBar()
{
    this->setComponentID("StatusBar");

    label_.setJustificationType(juce::Justification::left);
    label_.setEditable(false, false, false);

    this->addAndMakeVisible(label_);
}

void StatusBar::set_minimum_level(MessageLevel level)
{
    minimum_level_ = level;
}

void StatusBar::set_status(MessageLevel level, std::string text)
{
    current_level_ = level;

    if (current_level_ < minimum_level_)
    {
        return;
    }

    label_.setColour(juce::Label::textColourId,
                     this->findColour(get_color_id(current_level_)));
    label_.setFont(fonts::monospaced().regular.withHeight(18.f));
    label_.setText(text, juce::dontSendNotification);
}

void StatusBar::clear()
{
    label_.setText("", juce::dontSendNotification);
}

void StatusBar::resized()
{
    label_.setBounds(this->getLocalBounds());
}

void StatusBar::lookAndFeelChanged()
{
    label_.setColour(juce::Label::textColourId,
                     this->findColour(get_color_id(current_level_)));
    label_.setColour(juce::Label::backgroundColourId,
                     this->findColour(ColorID::Background));
}

void StatusBar::paintOverChildren(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::ForegroundLow));

    auto const bounds = this->getLocalBounds();
    g.drawHorizontalLine(bounds.getY(), (float)bounds.getX(), (float)bounds.getRight());
    g.drawHorizontalLine(bounds.getBottom(), (float)bounds.getX(),
                         (float)bounds.getRight());
}

} // namespace xen::gui