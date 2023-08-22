#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

namespace xen
{

/**
 * @brief Returns a reference to the selected Cell based on the given index vector.
 *
 * @param phrase The Phrase to select from.
 * @param indices A vector containing the indices to navigate through the nested
 * structure to the selected cell.
 * @return Cell& Reference to the selected Cell.
 *
 * @exception std::runtime_error If an index is out of bounds or if an invalid index
 * is provided for a non-Sequence Cell.
 */
[[nodiscard]] inline auto get_selected_cell(sequence::Phrase &phrase,
                                            SelectedState const &selected)
    -> sequence::Cell &
{
    // Start with the top-level Cell in the specified measure
    auto *current_cell = &phrase[selected.measure].cell;

    for (auto index : selected.cell)
    {
        // Assumption: current_cell must be a Sequence to navigate further
        if (std::holds_alternative<sequence::Sequence>(*current_cell))
        {
            current_cell = &std::get<sequence::Sequence>(*current_cell).cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return *current_cell;
}

/**
 * @brief Returns a reference to the selected Cell based on the given index vector.
 *
 * @param phrase The Phrase to select from.
 * @param indices A vector containing the indices to navigate through the nested
 * structure to the selected cell.
 * @return Cell const& Reference to the selected Cell.
 *
 * @exception std::runtime_error If an index is out of bounds or if an invalid index
 * is provided for a non-Sequence Cell.
 */
[[nodiscard]] inline auto get_selected_cell_const(sequence::Phrase const &phrase,
                                                  SelectedState const &selected)
    -> sequence::Cell const &
{
    // Start with the top-level Cell in the specified measure
    sequence::Cell const *current_cell = &phrase[selected.measure].cell;

    for (auto index : selected.cell)
    {
        // Assumption: current_cell must be a Sequence to navigate further
        if (std::holds_alternative<sequence::Sequence>(*current_cell))
        {
            current_cell = &std::get<sequence::Sequence>(*current_cell).cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return *current_cell;
}

/**
 * Utility to get the parent Sequence of the currently selected Cell.
 *
 * @param head The root sequence.
 * @param selected The current selection indices.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection is
 * at the top level.
 *
 * @throws std::runtime_error If any index is out of bounds.
 * @throws std::bad_variant_access If parent is not a Sequence.
 */
[[nodiscard]] inline auto get_parent_of_selected(sequence::Phrase const &phrase,
                                                 SelectedState const &selected)
    -> sequence::Sequence const *
{
    if (selected.cell.empty())
    {
        return nullptr;
    }

    sequence::Cell const *current_cell = &phrase[selected.measure].cell;

    for (std::size_t i = 0; i + 1 < selected.cell.size(); ++i)
    {
        current_cell =
            &(std::get<sequence::Sequence>(*current_cell).cells[selected.cell[i]]);
    }

    return &(std::get<sequence::Sequence>(*current_cell));
}

/**
 * Move the selection left within the current sequence.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving left.
 */
[[nodiscard]] inline auto move_left(sequence::Phrase const &phrase,
                                    SelectedState selected) -> SelectedState
{
    using namespace sequence::utility;

    return std::visit(
        overload{
            [&](sequence::Note const &) { return selected; },
            [&](sequence::Rest const &) { return selected; },
            [&](sequence::Sequence const &) {
                if (selected.cell.empty()) // Change Measure
                {
                    selected.measure = (selected.measure > 0) ? selected.measure - 1
                                                              : phrase.size() - 1;
                }
                else // Change Cell
                {
                    auto const parent_cells_size =
                        get_parent_of_selected(phrase, selected)->cells.size();

                    selected.cell.back() = (selected.cell.back() > 0)
                                               ? selected.cell.back() - 1
                                               : parent_cells_size - 1;
                }
                return selected;
            },
        },
        phrase[selected.measure].cell);
}

/**
 * Move the selection right within the current sequence.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving right.
 */
[[nodiscard]] inline auto move_right(sequence::Phrase const &phrase,
                                     SelectedState selected) -> SelectedState
{
    using namespace sequence::utility;

    return std::visit(
        overload{
            [&](sequence::Note const &) { return selected; },
            [&](sequence::Rest const &) { return selected; },
            [&](sequence::Sequence const &) {
                if (selected.cell.empty()) // Change Measure
                {
                    selected.measure = (selected.measure + 1 < phrase.size())
                                           ? selected.measure + 1
                                           : 0;
                }
                else // Change Cell
                {
                    auto const parent_cells_size =
                        get_parent_of_selected(phrase, selected)->cells.size();

                    selected.cell.back() =
                        (selected.cell.back() + 1 < parent_cells_size)
                            ? selected.cell.back() + 1
                            : 0;
                }
                return selected;
            },
        },
        phrase[selected.measure].cell);
}

/**
 * Move the selection up in the sequence hierarchy if possible.
 *
 * @param selected The current selection indices.
 * @return The new selection indices after moving up.
 */
[[nodiscard]] inline auto move_up(SelectedState selected) -> SelectedState
{
    if (!selected.cell.empty())
    {
        selected.cell.pop_back();
    }
    return selected;
}

/**
 * Move the selection down in the sequence hierarchy if possible.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving down, or the original indices if
 * moving down is not possible.
 */
[[nodiscard]] inline auto move_down(sequence::Phrase const &phrase,
                                    SelectedState selected) -> SelectedState
{
    auto const &selected_cell = get_selected_cell_const(phrase, selected);

    if (std::holds_alternative<sequence::Sequence>(selected_cell))
    {
        selected.cell.push_back(0);
    }

    return selected;
}

} // namespace xen