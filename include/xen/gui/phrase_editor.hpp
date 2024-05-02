#pragma once

#include <functional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include "heading.hpp"
#include "phrase.hpp"

namespace xen::gui
{

class PhraseEditor : public juce::Component
{
  public:
    Phrase phrase;

  public:
    PhraseEditor()
    {
        this->setComponentID("PhraseEditor");
        this->addAndMakeVisible(phrase);

        // Uses a key listener set up by XenEditor.
        this->setWantsKeyboardFocus(true);
    }

  protected:
    auto resized() -> void override
    {
        phrase.setBounds(this->getLocalBounds());
    }
};

} // namespace xen::gui