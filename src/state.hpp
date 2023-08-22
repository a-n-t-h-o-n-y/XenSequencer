#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <variant>
#include <vector>

#include <sequence/measure.hpp>
#include <sequence/modify.hpp>
#include <sequence/tuning.hpp>

namespace xen
{

/**
 * @brief The state of the internal sequencer for the plugin.
 */
struct State
{
    sequence::Phrase phrase;
    sequence::Tuning tuning;
    float base_frequency;
};

/**
 * @brief The state of the current selection in the sequencer.
 */
struct SelectedState
{
    /// The index of the currently selected Measure in the current Phrase.
    std::size_t measure;

    /// The index of the currently selected Cell in the current Measure.
    std::vector<std::size_t> cell{};
};

/**
 * @brief The state of the auxiliary controls in the plugin, for Timeline use.
 */
struct AuxState
{
    SelectedState selected;
    // TODO enum for input mode
};

/**
 * @brief The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

// bpm = 120.f,
// sample rate = 44'100,

[[nodiscard]] inline auto init_state() -> State
{
    namespace seq = sequence;
    return {
        seq::Phrase{seq::create_measure(seq::TimeSignature{4, 4})},
        seq::Tuning{
            {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
            1200,
        },
        440.f,
    };
}

/**
 * @brief Generates a demo state for testing.
 *
 * @return State
 */
[[nodiscard]] inline auto demo_state() -> State
{
    namespace seq = sequence;

    auto phrase = seq::Phrase{};

    for (auto i = 0; i < 2; ++i)
    {
        auto measure = seq::create_measure(seq::TimeSignature{4, 4});
        measure.cell = seq::generate::full(4, seq::Note{0, 1.0, 0.f, 0.8f});

        measure.cell = seq::modify::randomize_intervals(measure.cell, -40, 40);
        measure.cell = seq::modify::randomize_velocity(measure.cell, 0.f, 1.f);
        measure.cell = seq::modify::randomize_delay(measure.cell, 0.f, 0.1f);
        measure.cell = seq::modify::randomize_gate(measure.cell, 0.9f, 1.f);
        if (i % 2 != 1)
        {
            measure.cell = seq::modify::randomize_intervals(measure.cell, -20, 20);

            std::get<sequence::Sequence>(measure.cell).cells[3] = seq::Sequence{{
                seq::Note{5, 0.75f * (i / 2.f), 0.f, 0.3f},
                seq::Rest{},
                seq::Note{10, 0.5f * (i / 2.f), 0.25f, 1.f},
            }};
        }
        phrase.push_back(measure);
    }
    // {0, 25, 50, 75, 100, 125, 150},
    return State{
        phrase,
        seq::Tuning{
            {0, 200, 400, 500, 700, 900, 1100},
            1200,
        },
        440.f,
    };
}

} // namespace xen