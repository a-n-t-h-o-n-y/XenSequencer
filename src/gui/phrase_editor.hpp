#pragma once

#include <functional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include "heading.hpp"
#include "phrase.hpp"

namespace xen::gui
{

class CommandKeyListener : public juce::KeyListener
{
  public:
    CommandKeyListener(sl::Signal<void()> &on_bar_sig,
                       sl::Signal<void(std::string const &)> &on_command_sig)
        : on_command_bar_request(on_bar_sig), on_command(on_command_sig)
    {
    }

  protected:
    auto keyPressed(juce::KeyPress const &key, juce::Component *) -> bool override
    {
        // TODO you could create a separate class for this that lets you define
        // keypresses and command strings.
        auto const kc = key.getKeyCode();
        auto const modifiers = key.getModifiers();
        if (kc == ':')
        {
            on_command_bar_request();
            return true;
        }
        else if (kc == 'c' && modifiers.isCtrlDown())
        {
            on_command("copy");
            return true;
        }
        else if (kc == 'x' && modifiers.isCtrlDown())
        {
            on_command("cut");
            return true;
        }
        else if (kc == 'v' && modifiers.isCtrlDown())
        {
            on_command("paste");
            return true;
        }
        else if (kc == 'd' && modifiers.isCtrlDown())
        {
            on_command("duplicate");
            return true;
        }
        else if (kc == 'j' || kc == juce::KeyPress::downKey)
        {
            on_command("movedown");
            return true;
        }
        else if (kc == 'k' || kc == juce::KeyPress::upKey)
        {
            on_command("moveup");
            return true;
        }
        else if (kc == 'h' || kc == juce::KeyPress::leftKey)
        {
            on_command("moveleft");
            return true;
        }
        else if (kc == 'l' || kc == juce::KeyPress::rightKey)
        {
            on_command("moveright");
            return true;
        }
        else if (kc == 'm')
        {
            on_command("mode movement");
            return true;
        }
        else if (kc == 'n')
        {
            on_command("mode note");
            return true;
        }
        else if (kc == 'v')
        {
            on_command("mode velocity");
            return true;
        }
        else if (kc == 'd')
        {
            on_command("mode delay");
            return true;
        }
        else if (kc == 'g')
        {
            on_command("mode gate");
            return true;
        }
        return false;
    }

  private:
    sl::Signal<void()> &on_command_bar_request;
    sl::Signal<void(std::string const &)> &on_command;
};

class PhraseEditor : public juce::Component
{
  public:
    sl::Signal<void()> on_command_bar_request;

    sl::Signal<void(std::string const &)> on_command;

    Phrase phrase;

  public:
    PhraseEditor() : key_listener_{on_command_bar_request, on_command}
    {
        this->addAndMakeVisible(phrase);

        this->setWantsKeyboardFocus(true);
        this->addKeyListener(&key_listener_);
    }

  protected:
    auto resized() -> void override
    {
        phrase.setBounds(this->getLocalBounds());
    }

  private:
    CommandKeyListener key_listener_;
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