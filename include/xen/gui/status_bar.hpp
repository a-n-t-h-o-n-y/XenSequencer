#pragma once

#include <cctype>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>

namespace xen::gui
{

/**
 * Single line message display.
 */
class StatusBar : public juce::Component
{
  public:
    /**
     * Constructs a StatusBar with no text.
     */
    StatusBar()
    {
        this->setComponentID("StatusBar");

        label_.setJustificationType(juce::Justification::left);
        label_.setEditable(false, false, false);

        this->addAndMakeVisible(label_);
    }

  public:
    auto set_minimum_level(MessageLevel level) -> void
    {
        minimum_level_ = level;
    }

    auto set_status(MessageLevel level, std::string text) -> void
    {
        current_level_ = level;

        if (current_level_ < minimum_level_)
        {
            return;
        }

        label_.setColour(juce::Label::textColourId,
                         this->findColour((int)this->get_color_id(current_level_)));
        label_.setText(text, juce::dontSendNotification);
    }

    auto clear() -> void
    {
        label_.setText("", juce::dontSendNotification);
    }

  public:
    auto resized() -> void override
    {
        label_.setBounds(this->getLocalBounds());
    }

    auto lookAndFeelChanged() -> void override
    {
        label_.setColour(juce::Label::textColourId,
                         this->findColour((int)this->get_color_id(current_level_)));
        label_.setColour(juce::Label::backgroundColourId,
                         this->findColour((int)StatusBarColorIDs::Background));
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(this->findColour((int)StatusBarColorIDs::Outline));
        g.drawRect(this->getLocalBounds(), 1);
    }

  private:
    [[nodiscard]] static auto get_color_id(MessageLevel level) -> StatusBarColorIDs
    {
        switch (level)
        {
        case MessageLevel::Debug:
            return StatusBarColorIDs::DebugText;
        case MessageLevel::Info:
            return StatusBarColorIDs::InfoText;
        case MessageLevel::Warning:
            return StatusBarColorIDs::WarningText;
        case MessageLevel::Error:
            return StatusBarColorIDs::ErrorText;
        default:
            throw std::invalid_argument{
                "Invalid MessageLevel: " +
                std::to_string(
                    static_cast<std::underlying_type_t<MessageLevel>>(level))};
        }
    }

  private:
    juce::Label label_;
    MessageLevel minimum_level_{MessageLevel::Info};
    MessageLevel current_level_{MessageLevel::Info};
};

// class StatusBar : public juce::Component
// {
//   public:
//     StatusBar message_display;

//   public:
//     StatusBar() : input_mode_indicator{InputMode::Movement}
//     {
//         this->addAndMakeVisible(input_mode_indicator);
//         this->addAndMakeVisible(message_display);
//     }

//   protected:
//     auto resized() -> void override
//     {
//         auto flex = juce::FlexBox{};
//         flex.flexDirection = juce::FlexBox::Direction::row;

//         flex.items.add(juce::FlexItem{input_mode_indicator}
//                            .withWidth(InputModeIndicator::preferred_size)
//                            .withHeight(InputModeIndicator::preferred_size));
//         flex.items.add(juce::FlexItem{message_display}.withFlex(1));

//         flex.performLayout(this->getLocalBounds());
//     }

//     auto paint(juce::Graphics &g) -> void override
//     {
//         g.fillAll(this->findColour((int)StatusBarColorIDs::Background));
//     }

//     auto paintOverChildren(juce::Graphics &g) -> void override
//     {
//         g.setColour(this->findColour((int)StatusBarColorIDs::Outline));
//         g.drawRect(this->getLocalBounds(), 1);
//     }
// };

} // namespace xen::gui