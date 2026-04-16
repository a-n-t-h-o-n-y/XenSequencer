#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include <sequence/tuning.hpp>

#include <xen/chord.hpp>
#include <xen/clock.hpp>
#include <xen/command_history.hpp>
#include <xen/input_mode.hpp>
#include <xen/measure.hpp>
#include <xen/scale.hpp>
#include <xen/timeline.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

using SampleIndex = std::uint64_t;

using SampleCount = std::uint64_t;

using SequenceBank = std::array<Measure, 16>;

/**
 * The state of the sequencing engine.
 */
struct EngineState
{
    SequenceBank sequence_bank{};
    std::array<std::string, 16> sequence_names{};

    sequence::Tuning tuning{
        .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
        .octave = 1200,
        .description = "",
    };
    std::string tuning_name{"12-TET"};

    std::optional<Scale> scale{std::nullopt}; // Chromatic
    int key{0}; // The pitch considered 'zero', transposition. [0, tuning size)
    TranslateDirection scale_translate_direction{TranslateDirection::Up};

    float base_frequency{440.f};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif
    auto operator==(EngineState const &) const -> bool = default;
    auto operator!=(EngineState const &) const -> bool = default;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

/**
 * The state of the current selection in the sequencer.
 */
struct SelectedState
{
    /// The index of the currently selected Measure in the SequenceBank.
    std::size_t measure{0};

    /// The path to the currently selected Cell in the current Measure.
    std::vector<std::size_t> cell{};

    /// The index of the selected element within the selected Cell, if any.
    std::optional<std::size_t> element_index{};

    auto operator==(SelectedState const &other) const -> bool = default;
    auto operator!=(SelectedState const &other) const -> bool = default;
};

/**
 * The state of the arpeggiator held for cycling through chords.
 */
struct ArpState
{
    // The state of the sequencer when the arpeggiator was first used in a chain.
    EngineState sequencer{};
    SelectedState selected{};

    // The commit ID from just before the last arp call.
    int previous_commit_id{-1};

    // parameters for the arpeggiator
    std::string previous_chord_name{""};
    int previous_inversion{-1};
};

/**
 * Editor-facing state (selection and editing mode).
 */
struct EditorSessionState
{
    SelectedState selected{};
    InputMode input_mode = InputMode::Pitch;
    ArpState arp_state{};
};

/**
 * Explicit command execution context.
 *
 * @details Currently this is identical to EditorSessionState. Keep the alias so
 * command execution APIs can use context-centric naming without conversion overhead.
 */
using ExecutionContext = EditorSessionState;

struct TimelineState
{
    EngineState sequencer;
    EditorSessionState aux;
};

/**
 * The specific Timeline type for the Xen plugin.
 */
using XenTimeline = Timeline<TimelineState>;

struct AppConfigState
{
    juce::File current_sequence_directory = get_sequences_directory();
    juce::File current_tuning_directory = get_tunings_directory();
};

struct ContentLibraryState
{
    std::vector<Scale> scales{};
    std::optional<std::size_t> scale_shift_index{std::nullopt}; // null is chromatic
    std::vector<Chord> chords{};
};

enum class CommitIntent
{
    Auto,
    Defer,
    Force,
};

struct PluginState
{
    AppConfigState config{};
    ContentLibraryState library{};
    CommandHistory command_history{};
    CommitIntent commit_intent{CommitIntent::Auto};
    XenTimeline timeline;
};

/**
 * Snapshot passed from processor/core to UI readers.
 */
struct EngineSnapshot
{
    EngineState engine{};
    EditorSessionState editor{};
    int commit_id{-1};
    std::uint64_t snapshot_version{0};
};

/**
 * The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

struct AudioThreadStateForGUI
{
    DAWState daw;
    std::array<Clock::time_point, 16> note_start_times;
};

} // namespace xen
