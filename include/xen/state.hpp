#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>

#include <sequence/generate.hpp>
#include <sequence/measure.hpp>
#include <sequence/modify.hpp>
#include <sequence/tuning.hpp>

#include <xen/input_mode.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

struct Metadata
{
    std::string display_name;
};

/**
 * The state of the internal sequencer for the plugin.
 */
struct SequencerState
{
    sequence::Phrase phrase;
    sequence::Tuning tuning;
    float base_frequency;
};

/**
 * The state of the current selection in the sequencer.
 */
struct SelectedState
{
    /// The index of the currently selected Measure in the current Phrase.
    std::size_t measure;

    /// The index of the currently selected Cell in the current Measure.
    std::vector<std::size_t> cell{};
};

/**
 * The state of the auxiliary controls in the plugin, for Timeline use.
 */
struct AuxState
{
    SelectedState selected;
    InputMode input_mode = InputMode::Movement;
    juce::File current_phrase_directory{get_phrases_directory()};
    juce::String current_phrase_name{""};

    // TODO current_tuning_name
    // TODO current_tuning_directory
};

/**
 * The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

// bpm = 120.f,
// sample rate = 44'100,

[[nodiscard]] inline auto init_state() -> SequencerState
{
    return {
        .phrase = {{
            .cell = sequence::Rest{},
            .time_signature = {4, 4},
        }},
        .tuning =
            {
                .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                              1100},
                .octave = 1200,
            },
        .base_frequency = 440.f,
    };
}

/**
 * Generates a demo state for testing.
 *
 * @return SequencerState
 */
[[nodiscard]] inline auto demo_state() -> SequencerState
{
    namespace seq = sequence;

    auto phrase = seq::Phrase{};

    for (auto i = 0; i < 2; ++i)
    {
        auto measure = seq::create_measure(seq::TimeSignature{4, 4});
        measure.cell = seq::generate::full(4, seq::Note{0, 1.0, 0.f, 0.8f});

        measure.cell =
            seq::modify::randomize_intervals(measure.cell, {0, {1}}, -40, 40);
        measure.cell =
            seq::modify::randomize_velocity(measure.cell, {0, {1}}, 0.f, 1.f);
        measure.cell = seq::modify::randomize_delay(measure.cell, {0, {1}}, 0.f, 0.1f);
        measure.cell = seq::modify::randomize_gate(measure.cell, {0, {1}}, 0.9f, 1.f);
        if (i % 2 != 1)
        {
            measure.cell =
                seq::modify::randomize_intervals(measure.cell, {0, {1}}, -20, 20);

            std::get<sequence::Sequence>(measure.cell).cells[3] = seq::Sequence{{
                seq::Note{5, 0.75f, 0.f, 0.3f},
                seq::Rest{},
                seq::Note{10, 0.5f, 0.25f, 1.f},
            }};
        }
        phrase.push_back(measure);
    }
    // {0, 25, 50, 75, 100, 125, 150},
    return SequencerState{
        .phrase = phrase,
        .tuning =
            {
                .intervals = {0, 200, 400, 500, 700, 900, 1100},
                .octave = 1200,
            },
        .base_frequency = 440.f,
    };
}

} // namespace xen