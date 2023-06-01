#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "heading.hpp"
#include "phrase.hpp"

namespace xen::gui
{

// TODO
class PhraseEditor : public juce::Component
{
  public:
    PhraseEditor()
    {
        this->addAndMakeVisible(heading_);
        this->addAndMakeVisible(phrase_);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{heading_}.withHeight(30.0f));
        flexbox.items.add(juce::FlexItem{phrase_}.withFlex(1.0f));

        flexbox.performLayout(getLocalBounds());
    }

  private:
    Heading heading_{"Phrase Editor"};
    Phrase phrase_;
};

} // namespace xen::gui