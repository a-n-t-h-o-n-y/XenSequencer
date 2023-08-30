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
    sl::Signal<void()> on_command_bar_request;

    sl::Signal<void(std::string const &)> on_command;

    Phrase phrase;

  public:
    PhraseEditor()
    {
        this->addAndMakeVisible(phrase);

        this->setWantsKeyboardFocus(true);
    }

  protected:
    auto resized() -> void override
    {
        phrase.setBounds(this->getLocalBounds());
    }
};

} // namespace xen::gui