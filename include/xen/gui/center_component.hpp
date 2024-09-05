#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include <signals_light/signal.hpp>

#include <xen/double_buffer.hpp>
#include <xen/gui/accordion.hpp>
#include <xen/gui/library_view.hpp>
#include <xen/gui/sequence.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/state.hpp>

namespace xen::gui
{

/**
 * Label set up with colors for the CenterComponent
 */
class CenterComponentLabel : public juce::Label
{
  public:
    void lookAndFeelChanged() override;
};

template <typename Component>
class RightBordered : public Component
{
  public:
    using Component::Component;

  public:
    void paintOverChildren(juce::Graphics &g) override
    {
        // Draw a border on the right side of the label
        auto const border_thickness = 1;
        auto const width = this->getWidth();
        auto const height = this->getHeight();

        g.setColour(this->findColour(ColorID::ForegroundLow));
        g.fillRect(width - border_thickness, 0, border_thickness, height);
    }
};

// -------------------------------------------------------------------------------------

/**
 * key/value Label display where the value is editable.
 */
class FieldEdit : public juce::Component
{
  public:
    sl::Signal<void(juce::String const &)> on_text_change;

  public:
    FieldEdit(juce::String const &key = "", juce::String const &value = "");

  public:
    void set_key(juce::String const &key);

    void set_value(juce::String const &value);

    void set_font(juce::Font const &font);

  public:
    void resized() override;

  private:
    CenterComponentLabel key_;
    CenterComponentLabel value_;
    juce::String temp_text_; // Used to revert changes on edit in case of error.
};

// -------------------------------------------------------------------------------------

class MeasureInfo : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    explicit MeasureInfo();

  public:
    void update_ui(SequencerState const &state, AuxState const &aux);

  public:
    void resized() override;

    void paintOverChildren(juce::Graphics &g) override;

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
    IntervalColumn(std::size_t size, float vertical_offset);

  public:
    void update(std::size_t new_size);

    void paint(juce::Graphics &g) override;

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
    MeasureView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

    ~MeasureView() override;

  public:
    [[nodiscard]] auto get_cell() -> Cell &;

    [[nodiscard]] auto get_cell() const -> Cell const &;

    void update_ui(sequence::Measure const &measure, std::size_t tuning_size,
                   std::size_t selected_measure);

    /**
     * \p percent must be in range [0, 1).
     */
    void set_playhead(std::optional<float> percent);

  public:
    void resized() override;

    void paint(juce::Graphics &g) override;

    void paintOverChildren(juce::Graphics &g) override;

    void timerCallback() override;

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
    SequenceView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    void update_ui(SequencerState const &state, AuxState const &aux);

    void select(std::vector<std::size_t> const &indices);

  public:
    void resized() override;

  public:
    MeasureInfo measure_info;
    IntervalColumn interval_column;
    MeasureView measure_view;
    HAccordion<SequenceBankGrid> sequence_bank_accordion{"Sequence Bank"};
    SequenceBankGrid &sequence_bank = sequence_bank_accordion.child;
};

// -------------------------------------------------------------------------------------

class CenterComponent : public juce::Component
{
  public:
    SequenceView sequence_view;
    LibraryView library_view;

  public:
    CenterComponent(juce::File const &sequence_library_dir,
                    juce::File const &tuning_library_dir,
                    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    void show_sequence_view();

    void show_library_view();

    void update_ui(SequencerState const &state, AuxState const &aux);

  public:
    void resized() override;

  private:
    [[nodiscard]] auto current_component() -> juce::Component &;

  private:
    SequencerState state_;
};

} // namespace xen::gui