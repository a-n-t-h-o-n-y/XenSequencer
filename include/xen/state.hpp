#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
namespace juce
{
class LookAndFeel;
}

#include <sequence/generate.hpp>
#include <sequence/measure.hpp>
#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <signals_light/signal.hpp>

#include <xen/chord.hpp>
#include <xen/command_history.hpp>
#include <xen/gui/themes.hpp>
#include <xen/input_mode.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>
#include <xen/timeline.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

using SampleIndex = std::uint64_t;

using SampleCount = std::uint64_t;

using SequenceBank = std::array<sequence::Measure, 16>;

/**
 * The state of the internal sequencer for the plugin.
 */
struct SequencerState
{
    SequenceBank sequence_bank{};
    std::array<std::string, 16> measure_names{};

    sequence::Tuning tuning{
        .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
        .octave = 1200,
        .description = "",
    };
    std::string tuning_name{"12-TET"};

    std::optional<Scale> scale{std::nullopt}; // Chromatic
    int key{3}; // The pitch considered 'zero', transposition. [0, tuning size)
    TranslateDirection scale_translate_direction{TranslateDirection::Up};

    float base_frequency{440.f};
};

/**
 * The state of the current selection in the sequencer.
 */
struct SelectedState
{
    /// The index of the currently selected Measure in the SequenceBank.
    std::size_t measure{0};

    /// The index of the currently selected Cell in the current Measure.
    std::vector<std::size_t> cell{};

    auto operator==(SelectedState const &other) const -> bool = default;
    auto operator!=(SelectedState const &other) const -> bool = default;
};

/**
 * The state of the arpeggiator held for cycling through chords.
 */
struct ArpState
{
    // The state of the sequencer when the arpeggiator was first used in a chain.
    SequencerState sequencer{};
    SelectedState selected{};

    // The commit ID from just before the last arp call.
    int previous_commit_id{-1};

    // parameters for the arpeggiator
    std::string previous_chord_name{""};
    int previous_inversion{-1};
};

/**
 * The state of the auxiliary controls in the plugin, for Timeline use.
 */
struct AuxState
{
    SelectedState selected{};
    InputMode input_mode = InputMode::Pitch;
    ArpState arp_state{};
};

struct TrackedState
{
    SequencerState sequencer;
    AuxState aux;
};

/**
 * The specific Timeline type for the Xen plugin.
 */
using XenTimeline = Timeline<TrackedState>;

// -------------------------------------------------------------------------------------

/**
 * State shared across plugin instances if the DAW does not sandbox.
 */
struct SharedState
{
    sl::Signal<void()> on_load_keys_request{};
    std::mutex on_load_keys_request_mtx{};

    gui::Theme theme{gui::find_theme("apollo")}; // Needed for editor startup.
    sl::Signal<void(gui::Theme const &)> on_theme_update{};
    std::mutex theme_mtx{};
};

/**
 * The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

struct PluginState
{
    juce::File current_phrase_directory = get_sequences_directory();
    juce::File current_tuning_directory = get_tunings_directory();

    sl::Signal<void(std::string const &)> on_focus_request{};
    sl::Signal<void(std::string const &)> on_show_request{};
    CommandHistory command_history{};
    XenTimeline timeline;
    inline static SharedState shared{};
    std::unique_ptr<juce::LookAndFeel> laf{nullptr};
    std::vector<Scale> scales{};
    std::optional<std::size_t> scale_shift_index{std::nullopt};
    std::vector<Chord> chords{};
};

struct AudioThreadStateForGUI
{
    DAWState daw;
    SampleCount accumulated_sample_count;
    std::array<SampleIndex, 16> note_start_times;
};

} // namespace xen