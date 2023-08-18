#pragma once

#include <functional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include "heading.hpp"
#include "phrase.hpp"

namespace xen::gui
{

class CommandBarKeyListener : public juce::KeyListener
{
  public:
    CommandBarKeyListener(sl::Signal<void()> &signal) : on_command_bar_request(signal)
    {
    }

  protected:
    auto keyPressed(juce::KeyPress const &key, juce::Component *) -> bool override
    {
        if (key.getTextCharacter() == ':')
        {
            on_command_bar_request();
            return true;
        }
        return false;
    }

  private:
    sl::Signal<void()> &on_command_bar_request;
};

class PhraseEditor : public juce::Component
{
  public:
    sl::Signal<void()> on_command_bar_request;

    Phrase phrase;

  public:
    PhraseEditor() : keyListener{on_command_bar_request}
    {
        this->addAndMakeVisible(phrase);

        this->setWantsKeyboardFocus(true);
        this->addKeyListener(&keyListener);
    }

  protected:
    auto resized() -> void override
    {
        phrase.setBounds(this->getLocalBounds());
    }

  private:
    CommandBarKeyListener keyListener;
};

// class PhraseEditor : public juce::Component
// {
//   public:
//     PhraseEditor()
//     {
//         this->addAndMakeVisible(heading_);
//         this->addAndMakeVisible(phrase_);
//     }

//   public:
//     auto set_phrase(sequence::Phrase const &phrase) -> void
//     {
//         phrase_.set(phrase);
//     }

//     [[nodiscard]] auto get_phrase() -> sequence::Phrase
//     {
//         return phrase_.get_phrase();
//     }

//     auto set_tuning_length(std::size_t length) -> void
//     {
//         phrase_.set_tuning_length(length);
//     }

//   protected:
//     auto resized() -> void override
//     {
//         auto flexbox = juce::FlexBox{};
//         flexbox.flexDirection = juce::FlexBox::Direction::column;

//         flexbox.items.add(juce::FlexItem{heading_}.withHeight(30.0f));
//         flexbox.items.add(juce::FlexItem{phrase_}.withFlex(1.0f));

//         flexbox.performLayout(getLocalBounds());
//     }

//   private:
//     Heading heading_{"Phrase Editor"};
//     // TODO
//     // add 'add measure' button here. It should have a time sig editor and
//     // plus button and emit a measure when the plus button is pressed.
//     // (create_measure is called by it and passes the measure to be added to the
//     // state cache_)
//     Phrase phrase_;

//   public:
//     std::function<void()> &on_phrase_update = phrase_.on_update;
// };

} // namespace xen::gui