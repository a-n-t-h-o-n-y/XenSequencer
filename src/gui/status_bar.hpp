#pragma once

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../input_mode.hpp"

namespace xen::gui
{

/**
 * Displays a single letter representing the current InputMode.
 */
class ModeDisplay : public juce::Component
{
  public:
    static constexpr auto preferred_size = 23.f;

  public:
    /**
     * Constructs a ModeDisplay with a specific InputMode.
     *
     * @param input_mode The mode for which to display.
     */
    explicit ModeDisplay(InputMode mode)
    {
        this->addAndMakeVisible(label_);
        auto const font = juce::Font{juce::Font::getDefaultMonospacedFontName(), 16.f,
                                     juce::Font::bold};
        label_.setFont(font);
        this->set(mode);
    }

  public:
    auto set(InputMode mode) -> void
    {
        auto const first_letter = get_first_letter(mode);
        label_.setText(juce::String(std::string(1, first_letter)),
                       juce::dontSendNotification);
        label_.setJustificationType(juce::Justification::centred);
        label_.setColour(juce::Label::textColourId, get_color(mode));
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(juce::Colours::grey);
        g.drawRect(this->getLocalBounds(), 1);
    }

  private:
    [[nodiscard]] static auto get_first_letter(InputMode mode) -> char
    {
        auto const str = to_string(mode);
        assert(!str.empty());
        return static_cast<char>(std::toupper(str[0]));
    }

    [[nodiscard]] auto static get_color(InputMode mode) -> juce::Colour
    {
        switch (mode)
        {
        case InputMode::Movement:
            return juce::Colours::lightcoral;
        case InputMode::Note:
            return juce::Colours::lightgreen;
        case InputMode::Velocity:
            return juce::Colours::lightblue;
        case InputMode::Delay:
            return juce::Colours::lightgoldenrodyellow;
        case InputMode::Gate:
            return juce::Colours::lightpink;
        }
        throw std::invalid_argument{"Invalid input mode: " +
                                    std::to_string(static_cast<int>(mode))};
    }

  private:
    juce::Label label_;
};

/**
 * Single line message display.
 */
class MessageDisplay : public juce::Component
{
  public:
    /**
     * Constructs a MessageDisplay with no text.
     */
    MessageDisplay()
    {
        label_.setJustificationType(juce::Justification::left);
        label_.setEditable(false, false, false);

        this->addAndMakeVisible(label_);
    }

  public:
    auto set_info(std::string const &text) -> void
    {
        label_.setColour(juce::Label::textColourId, juce::Colours::white);
        label_.setText(text, juce::dontSendNotification);
    }

    auto set_warning(std::string const &text) -> void
    {
        label_.setColour(juce::Label::textColourId, juce::Colours::yellow);
        label_.setText(text, juce::dontSendNotification);
    }

    auto set_error(std::string const &text) -> void
    {
        label_.setColour(juce::Label::textColourId, juce::Colours::red);
        label_.setText(text, juce::dontSendNotification);
    }

    auto clear() -> void
    {
        label_.setText("", juce::dontSendNotification);
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(getLocalBounds());
    }

  private:
    juce::Label label_;
};

class StatusBar : public juce::Component
{
  public:
    ModeDisplay mode_display;
    MessageDisplay message_display;

  public:
    StatusBar() : mode_display{InputMode::Movement}
    {
        this->addAndMakeVisible(mode_display);
        this->addAndMakeVisible(message_display);
    }

  protected:
    auto resized() -> void override
    {
        auto flex = juce::FlexBox{};
        flex.flexDirection = juce::FlexBox::Direction::row;

        flex.items.add(juce::FlexItem{mode_display}
                           .withWidth(ModeDisplay::preferred_size)
                           .withHeight(ModeDisplay::preferred_size));
        flex.items.add(juce::FlexItem{message_display}.withFlex(1));

        flex.performLayout(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(juce::Colours::grey);
        g.drawRect(this->getLocalBounds(), 1);
    }
};

} // namespace xen::gui