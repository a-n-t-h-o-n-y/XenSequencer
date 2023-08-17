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
    std::size_t measure = 0;
    std::vector<std::size_t> cell = {0};
};

/**
 * @brief Returns a reference to the selected Cell based on the given index vector.
 * @param cells The top-level vector of Cells to navigate.
 * @param indices A vector containing the indices to navigate through the nested
 * structure.
 * @return Cell& Reference to the selected Cell.
 * @exception std::runtime_error If an index is out of bounds or if an invalid index
 * is provided for a non-Sequence Cell.
 */
[[nodiscard]] inline auto get_selected_cell(State &state, SelectedState const &selected)
    -> sequence::Cell &
{
    std::vector<sequence::Cell> *current_level =
        &(state.phrase[selected.measure].sequence.cells);

    for (auto i = 0u; i < selected.cell.size(); ++i)
    {
        auto index = selected.cell[i];

        if (index >= current_level->size())
        {
            throw std::runtime_error("Index out of bounds");
        }

        if (i + 1 < selected.cell.size())
        {
            // Expecting Sequence type if there are more indices to follow
            auto &cell = (*current_level)[index];
            if (std::holds_alternative<sequence::Sequence>(cell))
            {
                current_level = &std::get<sequence::Sequence>(cell).cells;
            }
            else
            {
                throw std::runtime_error("Expected a Sequence");
            }
        }
        else
        {
            // Last index in the indices vector; return the reference to the Cell
            return (*current_level)[index];
        }
    }

    throw std::runtime_error("No valid Cell found");
}

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
        440.f,
    };
}

} // namespace xen