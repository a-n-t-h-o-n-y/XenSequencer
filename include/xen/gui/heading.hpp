#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * A Label for displaying a heading.
 */
class Heading : public juce::Label
{
  public:
    /**
     * Construct a new Heading object
     *
     * @param text The text to display
     * @param padding The padding around the text
     * @param font The font to use
     */
    explicit Heading(juce::String const &text = juce::String{}, int padding = 10,
                     juce::Font font_ = juce::Font{"Arial", "Bold", 24.f},
                     juce::Justification just = juce::Justification::centred)
        : juce::Label{text, text}, padding_{padding}
    {
        this->setFont(font_);
        this->setText(text, juce::dontSendNotification);
        this->setColour(juce::Label::backgroundColourId, juce::Colours::black);
        this->setColour(juce::Label::textColourId, juce::Colours::white);
        this->setJustificationType(just);
    }

  public:
    /**
     * Set the text to display.
     *
     * @param text The text to display
     * @param notificationType The type of notification to send
     */
    auto setText(const juce::String &text, juce::NotificationType notificationType)
        -> void
    {
        this->juce::Label::setText(text, notificationType);

        float textHeight = this->getFont().getHeight();
        this->setSize(getWidth(), static_cast<int>(textHeight + 2.f * (float)padding_));
    }

  private:
    int padding_;
};

} // namespace xen::gui