#pragma once

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>

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

        this->lookAndFeelChanged();
    }

  public:
    auto set(InputMode mode) -> void
    {
        auto const first_letter = get_first_letter(mode);
        label_.setText(juce::String(std::string(1, first_letter)),
                       juce::dontSendNotification);
        label_.setJustificationType(juce::Justification::centred);
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(this->findColour((int)StatusBarColorIDs::Outline));
        g.drawRect(this->getLocalBounds(), 1);
    }

    auto lookAndFeelChanged() -> void override
    {
        label_.setColour(juce::Label::textColourId,
                         this->findColour((int)StatusBarColorIDs::ModeLetter));
    }

  private:
    [[nodiscard]] static auto get_first_letter(InputMode mode) -> char
    {
        auto const str = to_string(mode);
        assert(!str.empty());
        return static_cast<char>(std::toupper(str[0]));
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
    auto set_minimum_level(MessageLevel level) -> void
    {
        minimum_level_ = level;
    }

    auto set_status(MessageLevel level, std::string text) -> void
    {
        current_level_ = level;

        if (current_level_ < minimum_level_)
        {
            text.clear(); // So it will erase any leftover message.
        }

        label_.setColour(juce::Label::textColourId,
                         this->findColour((int)this->get_color_id(current_level_)));
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

    auto lookAndFeelChanged() -> void override
    {
        label_.setColour(juce::Label::textColourId,
                         this->findColour((int)this->get_color_id(current_level_)));
    }

  private:
    [[nodiscard]] static auto get_color_id(MessageLevel level) -> StatusBarColorIDs
    {
        switch (level)
        {
        case MessageLevel::Debug:
            return StatusBarColorIDs::DebugText;
        case MessageLevel::Info:
            return StatusBarColorIDs::InfoText;
        case MessageLevel::Warning:
            return StatusBarColorIDs::WarningText;
        case MessageLevel::Error:
            return StatusBarColorIDs::ErrorText;
        default:
            throw std::invalid_argument{
                "Invalid MessageLevel: " +
                std::to_string(
                    static_cast<std::underlying_type_t<MessageLevel>>(level))};
        }
    }

  private:
    juce::Label label_;
    MessageLevel minimum_level_{MessageLevel::Info};
    MessageLevel current_level_{MessageLevel::Info};
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

    auto paint(juce::Graphics &g) -> void override
    {
        g.fillAll(this->findColour((int)StatusBarColorIDs::Background));
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(this->findColour((int)StatusBarColorIDs::Outline));
        g.drawRect(this->getLocalBounds(), 1);
    }
};

} // namespace xen::gui