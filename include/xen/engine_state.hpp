#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include <sequence/measure.hpp>
#include <sequence/tuning.hpp>

#include <xen/chord.hpp>
#include <xen/clock.hpp>
#include <xen/input_mode.hpp>
#include <xen/scale.hpp>
#include <xen/timeline.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

using SampleIndex = std::uint64_t;
using SampleCount = std::uint64_t;
using SequenceBank = std::array<sequence::Measure, 16>;

struct SequencerState
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
    auto operator==(SequencerState const &) const -> bool = default;
    auto operator!=(SequencerState const &) const -> bool = default;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

struct SelectedState
{
    std::size_t measure{0};
    std::vector<std::size_t> cell{};

    auto operator==(SelectedState const &other) const -> bool = default;
    auto operator!=(SelectedState const &other) const -> bool = default;
};

struct ArpState
{
    SequencerState sequencer{};
    SelectedState selected{};
    int previous_commit_id{-1};
    std::string previous_chord_name{""};
    int previous_inversion{-1};
};

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

using XenTimeline = Timeline<TrackedState>;

struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

struct PluginState
{
    juce::File current_sequence_directory = get_sequences_directory();
    juce::File current_tuning_directory = get_tunings_directory();

    XenTimeline timeline;
    std::vector<Scale> scales{};
    std::optional<std::size_t> scale_shift_index{std::nullopt}; // null is chromatic
    std::vector<Chord> chords{};
};

struct AudioThreadStateForGUI
{
    DAWState daw;
    std::array<Clock::time_point, 16> note_start_times;
};

} // namespace xen
