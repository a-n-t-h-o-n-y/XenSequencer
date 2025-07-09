#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/tuning.hpp>

#include <signals_light/signal.hpp>

#include <xen/clock.hpp>
#include <xen/double_buffer.hpp>
#include <xen/gui/accordion.hpp>
#include <xen/gui/bg_sequence.hpp>
#include <xen/gui/cell.hpp>
#include <xen/gui/library_view.hpp>
#include <xen/gui/message_log.hpp>
#include <xen/gui/modulation_pane.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/gui/tuning_reference.hpp>
#include <xen/scale.hpp>
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
    FieldEdit(juce::String const &key = "", juce::String const &value = "",
              bool actually_editable = true);

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
    void update(SequencerState const &state, AuxState const &aux);

  public:
    void resized() override;

    void paintOverChildren(juce::Graphics &g) override;

  private:
    using BorderedFieldEdit = RightBordered<FieldEdit>;

    BorderedFieldEdit time_signature_;
    BorderedFieldEdit key_;
    BorderedFieldEdit base_frequency_;
    BorderedFieldEdit scale_;
    BorderedFieldEdit scale_mode_;
    BorderedFieldEdit tuning_name_;
    BorderedFieldEdit measure_name_;
};

// -------------------------------------------------------------------------------------

/**
 * Vertical column to display pitch numbers, [0, size) bottom to top, evenly spaced.
 */
class PitchColumn : public juce::Component
{
  public:
    PitchColumn(std::size_t size);

  public:
    void update(std::size_t new_size);

    void paint(juce::Graphics &g) override;

  private:
    std::size_t size_;
};

// -------------------------------------------------------------------------------------

/**
 * Draws playhead and owns the gui::Cell object.
 */
class MeasureView : public juce::Component, juce::Timer
{
  public:
    struct BGCurrentState
    {
        IR windowed_ir;
        float trigger_x_percent;
    };

  public:
    MeasureView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

    ~MeasureView() override;

  public:
    [[nodiscard]] auto get_cell() -> Cell &;

    [[nodiscard]] auto get_cell() const -> Cell const &;

    void update(SequencerState const &state, AuxState const &aux);

    /**
     * \p percent must be in range [0, 1).
     */
    void set_playhead(std::optional<float> percent);

    /**
     * Return the selected child cell.
     */
    [[nodiscard]] auto get_selected_child() -> Cell *;

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

    SequencerState sequencer_state_ = {.tuning_name = "repaint"}; // Force init paint.
    SelectedState selected_state_{};

    // BG Rendering
    std::array<std::optional<BGCurrentState>, 16> bg_current_;

    struct BGPreviousState
    {
        sequence::Tuning tuning;
        std::size_t bank_measure_selected;
        std::array<Clock::time_point, 16> note_start_times;
        std::array<IRWindow, 16> windows;
    } bg_previous_;

    // stored_windows_ is only used to determine if changes occured, not for IR.
    std::array<IRWindow, 16> stored_windows_;
};

// -------------------------------------------------------------------------------------

class SequenceView : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    SequenceView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    void update(SequencerState const &state, AuxState const &aux);

  public:
    void resized() override;

    void paintOverChildren(juce::Graphics &g) override;

  public:
    MeasureInfo measure_info;
    PitchColumn pitch_column;
    MeasureView measure_view;
    std::unique_ptr<TuningReference> tuning_reference_ptr{nullptr};
    HAccordion<SequenceBankGrid> sequence_bank_accordion{"Sequence Bank"};
    SequenceBankGrid &sequence_bank = sequence_bank_accordion.child;
    HAccordion<ModulationPane> modulation_pane_accordion{"Modulation"};
    ModulationPane &modulation_pane = modulation_pane_accordion.child;
};

// -------------------------------------------------------------------------------------

class CenterComponent : public juce::Component
{
  public:
    SequenceView sequence_view;
    LibraryView library_view;
    MessageLog message_log;

  public:
    CenterComponent(juce::File const &sequence_library_dir,
                    juce::File const &tuning_library_dir,
                    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    void show_sequence_view();

    void show_library_view();

    void show_message_log();

    void update(SequencerState const &state, AuxState const &aux,
                std::vector<Scale> const &scales);

  public:
    void resized() override;

  private:
    [[nodiscard]] auto current_component() -> juce::Component &;

  private:
    SequencerState state_;
};

} // namespace xen::gui