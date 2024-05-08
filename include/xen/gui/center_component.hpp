#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <signals_light/signal.hpp>

#include <xen/gui/library_view.hpp>
#include <xen/gui/sequence.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/string_manip.hpp>

namespace xen::gui
{

/**
 * Label set up with colors for the CenterComponent
 */
class CenterComponentLabel : public juce::Label
{
  public:
    explicit CenterComponentLabel(juce::String text = "") : juce::Label{text}
    {
        this->lookAndFeelChanged();
    }

  public:
    auto lookAndFeelChanged() -> void override
    {
        this->setColour(juce::Label::ColourIds::textColourId,
                        this->findColour((int)TimeSignatureColorIDs::Text));
        this->setColour(juce::Label::ColourIds::backgroundColourId,
                        this->findColour((int)TimeSignatureColorIDs::Background));
    }
};

template <typename Component>
class RightBordered : public Component
{
  public:
    using Component::Component;

  public:
    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        // Draw a border on the right side of the label
        auto const border_thickness = 1;
        auto const width = this->getWidth();
        auto const height = this->getHeight();

        g.setColour(this->findColour((int)TimeSignatureColorIDs::Outline));
        g.fillRect(width - border_thickness, 0, border_thickness, height);
    }
};

/**
 * key/value Label display where the value is editable.
 */
class FieldEdit : public juce::Component
{
  public:
    sl::Signal<void(juce::String const &)> on_text_change;

  public:
    FieldEdit(juce::String key = "", juce::String value = "")
    {
        this->addAndMakeVisible(key_);
        this->addAndMakeVisible(value_);

        value_.setEditable(false, true);
        value_.onEditorShow = [this] { temp_text_ = value_.getText(); };
        value_.onTextChange = [this] {
            auto const new_text = value_.getText();
            value_.setText(temp_text_, juce::dontSendNotification);
            this->on_text_change(new_text);
        };

        this->set_key(key);
        this->set_value(value);
    }

  public:
    auto set_key(juce::String const &key) -> void
    {
        key_.setText(key + ": ", juce::dontSendNotification);
    }

    auto set_value(juce::String const &value) -> void
    {
        value_.setText(value, juce::dontSendNotification);
    }

    auto set_font(juce::Font const &font) -> void
    {
        key_.setFont(font);
        value_.setFont(font);
    }

  public:
    auto resized() -> void override
    {
        auto flex_box = juce::FlexBox{};
        flex_box.flexDirection = juce::FlexBox::Direction::row;

        auto const width = (float)(key_.getFont().getStringWidth(key_.getText()) + 5);
        flex_box.items.add(juce::FlexItem{key_}.withWidth(width));
        flex_box.items.add(juce::FlexItem{value_}.withFlex(1));

        flex_box.performLayout(this->getLocalBounds());
    }

  private:
    CenterComponentLabel key_;
    CenterComponentLabel value_;
    juce::String temp_text_; // Used to revert changes on edit in case of error.
};

class MeasureInfo : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    explicit MeasureInfo()
    {
        this->setComponentID("MeasureInfo");

        this->addAndMakeVisible(time_signature_);
        this->addAndMakeVisible(base_frequency_);
        this->addAndMakeVisible(measure_name_);
        this->addAndMakeVisible(tuning_name_);

        time_signature_.on_text_change.connect([this](juce::String const &text) {
            this->on_command("set measure timesignature " + text.toStdString());
        });

        base_frequency_.on_text_change.connect([this](juce::String const &text) {
            this->on_command("set tuning basefrequency " + text.toStdString());
        });

        measure_name_.on_text_change.connect([this](juce::String const &text) {
            this->on_command("set measure name " + double_quote(text.toStdString()));
        });

        tuning_name_.on_text_change.connect([this](juce::String const &text) {
            this->on_command("set tuning name " + double_quote(text.toStdString()));
        });

        auto const font = juce::Font{juce::Font::getDefaultMonospacedFontName(), 14.f,
                                     juce::Font::plain};
        time_signature_.set_font(font);
        time_signature_.set_key("Time Signature");

        base_frequency_.set_font(font);
        base_frequency_.set_key("Base Frequency (Hz)");

        measure_name_.set_font(font);

        tuning_name_.set_font(font);
        tuning_name_.set_key("Tuning");
    }

  public:
    auto update_ui(SequencerState const &state, AuxState const &aux) -> void
    {
        {
            auto &measure = state.phrase[aux.selected.measure];
            auto const text = juce::String{measure.time_signature.numerator} + "/" +
                              juce::String{measure.time_signature.denominator};
            time_signature_.set_value(text);
        }

        {
            auto const text = juce::String{state.base_frequency};
            base_frequency_.set_value(text);
        }

        {
            auto const index = aux.selected.measure;
            measure_name_.set_key(juce::String{index});
            measure_name_.set_value(juce::String{state.measure_names[index]});
        }

        {
            tuning_name_.set_value(juce::String{state.tuning_name});
        }
    }

  public:
    auto resized() -> void override
    {
        auto flex_box = juce::FlexBox{};
        flex_box.flexDirection = juce::FlexBox::Direction::row;

        flex_box.items.add(juce::FlexItem{time_signature_}.withFlex(1));
        flex_box.items.add(juce::FlexItem{base_frequency_}.withFlex(1));
        flex_box.items.add(juce::FlexItem{measure_name_}.withFlex(1));
        flex_box.items.add(juce::FlexItem{tuning_name_}.withFlex(1));

        flex_box.performLayout(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(this->findColour((int)TimeSignatureColorIDs::Outline));
        g.drawRect(getLocalBounds(), 1);
    }

  private:
    using BorderedFieldEdit = RightBordered<FieldEdit>;

    BorderedFieldEdit time_signature_;
    BorderedFieldEdit base_frequency_;
    BorderedFieldEdit measure_name_;
    BorderedFieldEdit tuning_name_;
};

class SequenceView : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    SequenceView()
    {
        this->setComponentID("SequenceView");
        this->setWantsKeyboardFocus(true);

        this->addAndMakeVisible(measure_info_);
        this->addAndMakeVisible(sequence_bank_);

        measure_info_.on_command.connect(
            [this](std::string const &command) { this->on_command(command); });
    }

  public:
    auto update_ui(SequencerState const &state, AuxState const &aux) -> void
    {
        measure_info_.update_ui(state, aux);

        cell_ptr_ = make_cell(state.phrase[aux.selected.measure].cell,
                              state.tuning.intervals.size());
        this->addAndMakeVisible(*cell_ptr_);

        // TODO MeasureGrid

        this->resized();
    }

    auto select(std::vector<std::size_t> const &indices) -> void
    {
        if (cell_ptr_ != nullptr)
        {
            cell_ptr_->select_child(indices);
        }
    }

  public:
    auto resized() -> void override
    {
        auto flex_box = juce::FlexBox{};
        flex_box.flexDirection = juce::FlexBox::Direction::column;

        auto horizontal_flex = juce::FlexBox{};
        horizontal_flex.flexDirection = juce::FlexBox::Direction::row;
        horizontal_flex.items.add(juce::FlexItem{*cell_ptr_}.withFlex(1));
        // TODO figure out how to make square
        horizontal_flex.items.add(juce::FlexItem{sequence_bank_}.withWidth(300));

        if (cell_ptr_ != nullptr)
        {
            flex_box.items.add(juce::FlexItem{measure_info_}.withHeight(23.f));
            flex_box.items.add(juce::FlexItem{horizontal_flex}.withFlex(1));
        }

        flex_box.performLayout(this->getLocalBounds());
    }

  private:
    [[nodiscard]] static auto make_cell(sequence::Cell const &cell,
                                        std::size_t tuning_octave_size)
        -> std::unique_ptr<Cell>
    {
        auto const builder = BuildAndAllocateCell{tuning_octave_size};

        return std::visit(builder, cell);
    }

  private:
    MeasureInfo measure_info_;
    std::unique_ptr<Cell> cell_ptr_;
    SequenceBank sequence_bank_;
    // TODO sequencebank in accordion
};

class CenterComponent : public juce::Component
{
  public:
    SequenceView sequence_view;
    LibraryView library_view;

  public:
    CenterComponent(juce::File const &sequence_library_dir,
                    juce::File const &tuning_library_dir)
        : library_view{sequence_library_dir, tuning_library_dir}
    {
        this->addAndMakeVisible(sequence_view);
        this->addChildComponent(library_view);
    }

  public:
    auto show_sequence_view() -> void
    {
        sequence_view.setVisible(true);
        library_view.setVisible(false);
        this->resized();
    }

    auto show_library_view() -> void
    {
        sequence_view.setVisible(false);
        library_view.setVisible(true);
        this->resized();
    }

    auto update_ui(SequencerState const &state, AuxState const &aux)
    {
        state_ = state;
        sequence_view.update_ui(state_, aux);

        // TODO update library view? Is there anything? Are current directories updated
        // via a separate mechanism?
    }

  public:
    auto resized() -> void override
    {
        this->current_component().setBounds(this->getLocalBounds());
    }

  private:
    auto current_component() -> juce::Component &
    {
        if (sequence_view.isVisible())
        {
            return sequence_view;
        }
        else if (library_view.isVisible())
        {
            return library_view;
        }
        else
        {
            assert(false);
        }
    }

  private:
    SequencerState state_;
};

} // namespace xen::gui