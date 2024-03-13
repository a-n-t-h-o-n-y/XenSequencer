#pragma once

/**
 * A component that can be given focus and visually display it.
 *
 * @details Focus can be given by tab focus cycling or left mouse click.
 */
class FocusableComponent : public juce::Component
{
  public:
    FocusableComponent()
    {
        this->setWantsKeyboardFocus(true);
    }

  protected:
    void mouseDown(juce::MouseEvent const &) override
    {
        this->grabKeyboardFocus();
    }

    void focusGained(juce::Component::FocusChangeType) override
    {
        repaint();
    }

    void focusLost(juce::Component::FocusChangeType) override
    {
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        if (this->hasKeyboardFocus(false))
        {
            g.setColour(juce::Colours::yellow);
            g.drawRect(getLocalBounds(), 3);
        }
    }
};
