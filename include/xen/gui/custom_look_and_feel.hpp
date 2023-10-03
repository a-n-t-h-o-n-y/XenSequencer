#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    void drawLabel(juce::Graphics &g, juce::Label &label) override
    {
        g.fillAll(label.findColour(juce::TextEditor::backgroundColourId));

        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font(getLabelFont(label));

            g.setColour(
                label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(font);

            juce::Rectangle<int> textArea(
                label.getBorderSize().subtractedFrom(label.getLocalBounds()));

            g.drawFittedText(
                label.getText(), textArea, label.getJustificationType(),
                juce::jmax(1, (int)(textArea.getHeight() / font.getHeight())),
                label.getMinimumHorizontalScale());

            g.setColour(label.findColour(juce::TextEditor::outlineColourId)
                            .withMultipliedAlpha(alpha));
        }
        else if (label.isEnabled())
        {
            g.setColour(label.findColour(juce::Label::outlineColourId));
        }

        g.drawRect(label.getLocalBounds());
    }
};

} // namespace xen::gui