#pragma once

#include <functional>

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

  public:
    auto set_phrase(sequence::Phrase const &phrase) -> void
    {
        phrase_.set(phrase);
    }

    [[nodiscard]] auto get_phrase() -> sequence::Phrase
    {
        return phrase_.get_phrase();
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

  public:
    std::function<void()> &on_phrase_update = phrase_.on_update;
};

} // namespace xen::gui