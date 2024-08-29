#include <xen/gui/status_bar.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>

#include <xen/gui/color_ids.hpp>
#include <xen/message_level.hpp>

namespace
{

[[nodiscard]] auto get_color_id(xen::MessageLevel level) -> xen::gui::StatusBarColorIDs
{
    using namespace xen;
    switch (level)
    {
    case MessageLevel::Debug:
        return gui::StatusBarColorIDs::DebugText;
    case MessageLevel::Info:
        return gui::StatusBarColorIDs::InfoText;
    case MessageLevel::Warning:
        return gui::StatusBarColorIDs::WarningText;
    case MessageLevel::Error:
        return gui::StatusBarColorIDs::ErrorText;
    default:
        throw std::invalid_argument{
            "Invalid MessageLevel: " +
            std::to_string(static_cast<std::underlying_type_t<MessageLevel>>(level))};
    }
}

} // namespace

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
                     this->findColour((int)get_color_id(current_level_)));
    // TODO set font
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
                     this->findColour((int)get_color_id(current_level_)));
    label_.setColour(juce::Label::backgroundColourId,
                     this->findColour((int)StatusBarColorIDs::Background));
}

void StatusBar::paintOverChildren(juce::Graphics &g)
{
    g.setColour(this->findColour((int)StatusBarColorIDs::Outline));
    g.drawRect(this->getLocalBounds(), 1);
}

} // namespace xen::gui