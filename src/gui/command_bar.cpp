#include <xen/gui/command_bar.hpp>

#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/pattern.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

namespace xen::gui
{

CommandInputComponent::CommandInputComponent()
{
    this->setWantsKeyboardFocus(true);
    this->setMultiLine(false, false);
    this->setReturnKeyStartsNewLine(false);
    this->setEscapeAndReturnKeysConsumed(true);
}

auto CommandInputComponent::is_cursor_at_end() const -> bool
{
    return this->getCaretPosition() == this->getText().length();
}

auto CommandInputComponent::keyPressed(juce::KeyPress const &key) -> bool
{
    if (key == juce::KeyPress::tabKey && onTabKey)
    {
        return onTabKey();
    }
    else if (key == juce::KeyPress::upKey && onArrowUpKey)
    {
        return onArrowUpKey();
    }
    else if (key == juce::KeyPress::downKey && onArrowDownKey)
    {
        return onArrowDownKey();
    }

    return juce::TextEditor::keyPressed(key);
}

void CommandInputComponent::focusLost(juce::Component::FocusChangeType cause)
{
    if (cause != juce::Component::FocusChangeType::focusChangedDirectly && focus_lost)
    {
        this->focus_lost();
    }
}

// -------------------------------------------------------------------------------------

CommandBar::CommandBar(CommandHistory &cmd_history) : command_history_{cmd_history}
{
    this->setComponentID("CommandBar");
    this->setWantsKeyboardFocus(false);

    this->addAndMakeVisible(command_input_);
    this->addAndMakeVisible(ghost_text_);

    ghost_text_.setMultiLine(false, false);
    ghost_text_.setReadOnly(true);
    ghost_text_.setEnabled(false);
    ghost_text_.setInterceptsMouseClicks(false, false);
    ghost_text_.setWantsKeyboardFocus(false);
    ghost_text_.setOpaque(false);

    command_input_.onReturnKey = [this] {
        this->do_send_command();
        this->clear();
        this->close();
    };
    command_input_.onTextChange = [this] {
        // Part I
        this->add_guide_text();

        // Part II
        // Emit Pattern
        auto const pattern = this->extract_pattern_from_content();
        if (pattern != last_pattern_)
        {
            last_pattern_ = pattern;
            this->on_pattern_update(last_pattern_);
        }
    };
    command_input_.onEscapeKey = [this] { this->close(); };
    command_input_.onTabKey = [this] {
        this->do_tab_press();
        return true;
    };
    command_input_.onArrowDownKey = [this] {
        this->do_history_next();
        return true;
    };
    command_input_.onArrowUpKey = [this] {
        this->do_history_previous();
        return true;
    };

    command_input_.focus_lost = [this] { 
        this->on_command("show StatusBar"); 
        };

    auto const font = fonts::monospaced().regular.withHeight(18.f);
    command_input_.setFont(font);
    ghost_text_.setFont(font);
}

void CommandBar::clear()
{
    command_input_.setText("");
    ghost_text_.setText("");
}

void CommandBar::focus()
{
    command_input_.grabKeyboardFocus();
}

void CommandBar::close()
{
    this->clear();
    this->on_command("show StatusBar;focus SequenceView");
}

auto CommandBar::extract_pattern_from_content() const -> sequence::Pattern
{
    auto const text = command_input_.getText().toStdString();
    if (sequence::contains_valid_pattern(text))
    {
        return sequence::parse_pattern(text);
    }
    else
    {
        return {0, {1}};
    }
}

void CommandBar::resized()
{
    ghost_text_.setBounds(this->getLocalBounds());
    command_input_.setBounds(this->getLocalBounds());
}

void CommandBar::lookAndFeelChanged()
{
    auto const bg = this->findColour(ColorID::BackgroundMedium);
    auto const text = this->findColour(ColorID::ForegroundHigh);
    auto const ghost = this->findColour(ColorID::ForegroundLow);
    auto const outline = this->findColour(ColorID::ForegroundLow);

    command_input_.setColour(juce::TextEditor::backgroundColourId, bg);
    command_input_.setColour(juce::TextEditor::textColourId, text);
    command_input_.setColour(juce::TextEditor::focusedOutlineColourId, outline);
    command_input_.setColour(juce::TextEditor::outlineColourId, outline);

    ghost_text_.setColour(juce::TextEditor::textColourId, ghost);
    ghost_text_.setColour(juce::TextEditor::backgroundColourId,
                          juce::Colours::transparentWhite);
}

void CommandBar::do_send_command()
{
    auto const command = command_input_.getText().toStdString();
    command_history_.add_command(command);
    this->on_command(command);
}

void CommandBar::add_guide_text()
{
    auto const input = command_input_.getText().toStdString();

    auto const gt = this->on_guide_text_request(input);
    if (gt.has_value())
    {
        ghost_text_.setText(std::string(input.size(), ' ') + *gt,
                            juce::NotificationType::dontSendNotification);
    }
}

void CommandBar::do_tab_press()
{
    if (!command_input_.is_cursor_at_end())
    {
        return;
    }

    auto const input = command_input_.getText().toStdString();
    auto const id = this->on_complete_id_request(input);
    if (id.has_value())
    {
        auto const completed_text = input + *id + (id->empty() ? "" : " ");
        command_input_.setText(completed_text,
                               juce::NotificationType::dontSendNotification);
        ghost_text_.clear();
        this->add_guide_text();
    }
}

void CommandBar::do_history_next()
{
    auto const cmd = command_history_.next();
    if (cmd)
    {
        command_input_.setText(*cmd);
    }
    else
    {
        // TODO use saved typing buffer
        command_input_.setText("");
    }
}

void CommandBar::do_history_previous()
{
    auto const cmd = command_history_.previous();
    if (cmd)
    {
        command_input_.setText(*cmd);
    }
    else
    {
        // TODO use saved typing buffer
        command_input_.setText("");
    }
}

} // namespace xen::gui