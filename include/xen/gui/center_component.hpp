#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <signals_light/signal.hpp>

#include <xen/double_buffer.hpp>
#include <xen/gui/accordion.hpp>
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
            auto &measure = state.sequence_bank[aux.selected.measure];
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

// -------------------------------------------------------------------------------------

/**
 * Vertical column to display interval numbers, [0, size) bottom to top, evenly spaced.
 */
class IntervalColumn : public juce::Component
{
  public:
    IntervalColumn(std::size_t size, float vertical_offset)
        : size_{size}, vertical_offset_{vertical_offset}
    {
    }

  public:
    auto update(std::size_t new_size) -> void
    {
        size_ = new_size;
    }

    auto paint(juce::Graphics &g) -> void override
    {
        // TODO color ID
        g.fillAll(this->findColour((int)MeasureColorIDs::Background));

        auto const bounds =
            this->getLocalBounds().toFloat().reduced(0.f, vertical_offset_);

        // TODO add color ID
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font{
            juce::Font::getDefaultMonospacedFontName(),
            14.f,
            juce::Font::plain,
        });

        auto const item_height = bounds.getHeight() / static_cast<float>(size_);

        for (std::size_t i = 0; i < size_; ++i)
        {
            float y = bounds.getBottom() - (static_cast<float>(i) + 1.f) * item_height;
            auto const text = juce::String(i).paddedLeft('0', 2);

            g.drawText(text, bounds.withY(y).withHeight(item_height),
                       juce::Justification::centred, true);
        }
    }

  private:
    std::size_t size_;
    float vertical_offset_;
};

// -------------------------------------------------------------------------------------

/**
 * Draws playhead and owns the gui::Cell object.
 */
class MeasureView : public juce::Component, juce::Timer
{
  public:
    MeasureView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
        : cell_ptr_{make_cell(sequence::Rest{}, 12)},
          audio_thread_state_{audio_thread_state}
    {
        this->set_playhead(0.75f);
        this->startTimer(33);
    }

  public:
    [[nodiscard]] auto get_cell() -> Cell &
    {
        return *cell_ptr_;
    }

    [[nodiscard]] auto get_cell() const -> Cell const &
    {
        return *cell_ptr_;
    }

    auto update_ui(sequence::Measure const &measure, std::size_t tuning_size,
                   std::size_t selected_measure) -> void
    {
        // TODO can you do a simple equality check to conditionally rebuild the cell?
        cell_ptr_.reset();
        cell_ptr_ = make_cell(measure.cell, tuning_size);
        this->addAndMakeVisible(*cell_ptr_);
        selected_measure_ = selected_measure;
        measure_ = measure;
        this->resized();
    }

    /**
     * \p percent must be in range [0, 1).
     */
    auto set_playhead(std::optional<float> percent) -> void
    {
        if (percent.has_value())
        {
            assert(*percent >= 0.f && *percent < 1.f);
        }
        playhead_ = percent;
        this->repaint();
    }

  public:
    auto resized() -> void override
    {
        cell_ptr_->setBounds(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        if (playhead_.has_value())
        {
            // TODO create a new theme entry for playhead.
            g.setColour(this->findColour((int)MeasureColorIDs::SelectionHighlight));

            auto const x_pos = playhead_.value() * static_cast<float>(this->getWidth());
            g.drawLine(x_pos, 4.f, x_pos, static_cast<float>(this->getHeight() - 4));
        }
    }

    auto timerCallback() -> void override
    {
        auto const audio_thread_state = audio_thread_state_.read();
        if (audio_thread_state.note_start_times[selected_measure_] != (std::uint64_t)-1)
        {
            auto const samples_in_measure =
                sequence::samples_count(measure_, audio_thread_state.daw.sample_rate,
                                        audio_thread_state.daw.bpm);
            auto const current_sample = audio_thread_state.accumulated_sample_count;
            auto const start_sample =
                audio_thread_state.note_start_times[selected_measure_];
            auto const percent =
                (float)((current_sample - start_sample) % samples_in_measure) /
                (float)samples_in_measure;
            this->set_playhead(percent);
        }
        else
        {
            this->set_playhead(std::nullopt);
        }
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
    std::unique_ptr<Cell> cell_ptr_; // Never Null
    std::optional<float> playhead_ = std::nullopt;

    // Owned by XenProcessor
    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state_;

    sequence::Measure measure_;
    std::size_t selected_measure_{0};
};

// -------------------------------------------------------------------------------------

class SequenceView : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    SequenceView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
        : interval_column{12, 4.f}, measure_view{audio_thread_state}
    {
        this->setComponentID("SequenceView");
        this->setWantsKeyboardFocus(true);

        this->addAndMakeVisible(measure_info);
        this->addAndMakeVisible(sequence_bank_accordion);
        this->addAndMakeVisible(interval_column);
        this->addAndMakeVisible(measure_view);

        measure_info.on_command.connect(
            [this](std::string const &command) { this->on_command(command); });
    }

  public:
    auto update_ui(SequencerState const &state, AuxState const &aux) -> void
    {
        measure_info.update_ui(state, aux);

        measure_view.update_ui(state.sequence_bank[aux.selected.measure],
                               state.tuning.intervals.size(), aux.selected.measure);

        interval_column.update(state.tuning.intervals.size());

        sequence_bank.update_ui(aux.selected.measure);

        this->resized();
    }

    auto select(std::vector<std::size_t> const &indices) -> void
    {
        measure_view.get_cell().select_child(indices);
    }

  public:
    auto resized() -> void override
    {
        sequence_bank_accordion.set_flexitem(
            juce::FlexItem{}.withWidth((float)this->getHeight()));

        auto flex_box = juce::FlexBox{};
        flex_box.flexDirection = juce::FlexBox::Direction::column;

        auto horizontal_flex = juce::FlexBox{};
        horizontal_flex.flexDirection = juce::FlexBox::Direction::row;
        horizontal_flex.items.add(juce::FlexItem{interval_column}.withWidth(23.f));
        horizontal_flex.items.add(juce::FlexItem{measure_view}.withFlex(1));
        // TODO figure out how to make square
        horizontal_flex.items.add(sequence_bank_accordion.get_flexitem());

        flex_box.items.add(juce::FlexItem{measure_info}.withHeight(23.f));
        flex_box.items.add(juce::FlexItem{horizontal_flex}.withFlex(1));

        flex_box.performLayout(this->getLocalBounds());
    }

  public:
    MeasureInfo measure_info;
    IntervalColumn interval_column;
    MeasureView measure_view;
    HAccordion<SequenceBankGrid> sequence_bank_accordion{"Sequence Bank"};
    SequenceBankGrid &sequence_bank = sequence_bank_accordion.child;
};

class CenterComponent : public juce::Component
{
  public:
    SequenceView sequence_view;
    LibraryView library_view;

  public:
    CenterComponent(juce::File const &sequence_library_dir,
                    juce::File const &tuning_library_dir,
                    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
        : sequence_view{audio_thread_state},
          library_view{sequence_library_dir, tuning_library_dir}
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