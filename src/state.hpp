#pragma once

#include <cstdint>

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
    float bpm;
    std::uint32_t sample_rate;
    float base_frequency;
};

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
        measure.sequence = seq::generate::full(4, seq::Note{0, 1.0, 0.f, 0.8f});
        measure.sequence = seq::modify::randomize_intervals(measure.sequence, -40, 40);
        measure.sequence = seq::modify::randomize_velocity(measure.sequence, 0.f, 1.f);
        measure.sequence = seq::modify::randomize_delay(measure.sequence, 0.f, 0.1f);
        measure.sequence = seq::modify::randomize_gate(measure.sequence, 0.9f, 1.f);
        if (i % 2 != 0)
        {
            measure.sequence =
                seq::modify::randomize_intervals(measure.sequence, -20, 20);

            measure.sequence.cells[3] = seq::Sequence{{
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
        120.f,
        44'100,
        440.f,
    };
}

[[nodiscard]] inline auto init_state() -> State
{
    namespace seq = sequence;
    return {
        seq::Phrase{seq::create_measure(seq::TimeSignature{7, 9})},
        seq::Tuning{
            {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
            1200,
        },
        120.f,
        44'100,
        440.f,
    };
}

} // namespace xen