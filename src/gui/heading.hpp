#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief A Label for displaying a heading.
 */
class Heading : public juce::Label
{
  public:
    /**
     * @brief Construct a new Heading object
     *
     * @param text The text to display
     * @param padding The padding around the text
     * @param font The font to use
     */
    explicit Heading(juce::String const &text = juce::String{}, int padding = 10,
                     juce::Font font = juce::Font{"Arial", "Bold", 24.f})
        : juce::Label{text, text}, padding_{padding}, font_{font}
    {
        this->setFont(font_);
        this->setText(text, juce::dontSendNotification);
        this->setColour(juce::Label::backgroundColourId, juce::Colours::black);
        this->setColour(juce::Label::textColourId, juce::Colours::white);
    }

  public:
    /**
     * @brief Set the text to display.
     *
     * @param text The text to display
     * @param notificationType The type of notification to send
     */
    auto setText(const juce::String &text, juce::NotificationType notificationType)
        -> void
    {
        this->juce::Label::setText(text, notificationType);

        float textHeight = this->getFont().getHeight();
        this->setSize(getWidth(), static_cast<int>(textHeight + 2.f * padding_));
    }

  private:
    int padding_;
    juce::Font font_;
};
