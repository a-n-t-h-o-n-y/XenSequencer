#pragma once

#include <cstddef>

#include <sequence/sequence.hpp>

#include <xen/state.hpp>

namespace xen
{

enum class SelectionKind
{
    Cell,
    Element,
};

[[nodiscard]] auto selection_kind(SelectedState const &selected) -> SelectionKind;

[[nodiscard]] auto select_element_in_cell(SelectedState selected, std::size_t index)
    -> SelectedState;

[[nodiscard]] auto select_sequence_cell(SelectedState selected, std::size_t index)
    -> SelectedState;

[[nodiscard]] auto select_parent_cell(SelectedState selected) -> SelectedState;

/**
 * Resolve the current selection to a Cell.
 *
 * @param measure The root Measure to select from.
 * @param selected The current typed path selection state.
 * @return Cell& to the selected Cell.
 * @exception std::runtime_error If the selection does not resolve to a Cell.
 */
[[nodiscard]] auto get_selected_cell(Measure &measure,
                                     SelectedState const &selected) -> sequence::Cell &;

/**
 * Resolve the current selection to a Cell.
 *
 * @param measure The root Measure to select from.
 * @param selected The current typed path selection state.
 * @return Cell const& to the selected Cell.
 * @exception std::runtime_error If the selection does not resolve to a Cell.
 */
[[nodiscard]] auto get_selected_cell_const(
    Measure const &measure, SelectedState const &selected) -> sequence::Cell const &;

[[nodiscard]] auto get_selected_element(Measure &measure,
                                        SelectedState const &selected)
    -> sequence::MusicElement &;

[[nodiscard]] auto get_selected_element_const(
    Measure const &measure, SelectedState const &selected)
    -> sequence::MusicElement const &;

[[nodiscard]] auto get_selected_sequence(Measure &measure,
                                         SelectedState const &selected)
    -> sequence::Sequence &;

[[nodiscard]] auto get_selected_sequence_const(
    Measure const &measure, SelectedState const &selected)
    -> sequence::Sequence const &;

[[nodiscard]] auto get_selected_element_index(SelectedState const &selected)
    -> std::size_t;

[[nodiscard]] auto get_selected_cell_index(SelectedState const &selected)
    -> std::size_t;

/**
 * Get the parent Cell of the selected child Cell.
 *
 * @param measure The root Measure to select from.
 * @param selected The current selection state.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection
 * is at the top level.
 */
[[nodiscard]] auto get_parent_of_selected(
    Measure &measure, SelectedState const &selected) -> sequence::Cell *;

/**
 * Get the parent Cell of the selected child Cell.
 *
 * @param measure The root Measure to select from.
 * @param selected The current selection state.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection
 * is at the top level.
 */
[[nodiscard]] auto get_parent_of_selected_const(
    Measure const &measure, SelectedState const &selected) -> sequence::Cell const *;

[[nodiscard]] auto get_parent_sequence_of_selected_cell(
    Measure &measure, SelectedState const &selected) -> sequence::Sequence *;

[[nodiscard]] auto get_parent_sequence_of_selected_cell_const(
    Measure const &measure, SelectedState const &selected)
    -> sequence::Sequence const *;

[[nodiscard]] auto get_parent_cell_of_selection(Measure &measure,
                                                SelectedState const &selected)
    -> sequence::Cell *;

[[nodiscard]] auto get_parent_cell_of_selection_const(
    Measure const &measure, SelectedState const &selected)
    -> sequence::Cell const *;

/**
 * Utility to get the number of siblings of the currently selected Cell.
 *
 * @details This count is the total number of child cells of the selected cell's parent.
 * This includes the selected cell itself.
 * @param measure The root Measure to select from.
 * @param selected The current selection state.
 * @return The number of siblings of the selected Cell.
 */
[[nodiscard]] auto get_sibling_count(Measure const &measure,
                                     SelectedState const &selected) -> std::size_t;

/**
 * Move the selection left within the current container.
 *
 * @param measure The root Measure to work with.
 * @param selected The current selection state.
 * @param amount The number of positions to move left.
 * @return The new selection after moving left.
 */
[[nodiscard]] auto move_left(Measure const &measure, SelectedState selected,
                             std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection right within the current container.
 *
 * @param measure The root Measure to work with.
 * @param selected The current selection state.
 * @param amount The number of positions to move right.
 * @return The new selection after moving right.
 */
[[nodiscard]] auto move_right(Measure const &measure, SelectedState selected,
                              std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection up one logical level if possible.
 *
 * @details If the selection is inside a cell whose only element is a Sequence,
 * this skips back to the containing Cell instead of stopping on the Sequence
 * element.
 *
 * @param measure The root Measure to work with.
 * @param selected The current selection state.
 * @param amount The number of steps to move up.
 * @return The new selection path after moving up.
 */
[[nodiscard]] auto move_up(Measure const &measure, SelectedState selected,
                           std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection down into the first element of the current Cell, then into
 * child Cells of selected Sequence elements if possible.
 *
 * @details If the current Cell contains exactly one element and that element is a
 * Sequence, this skips directly into the Sequence's first child Cell. If the
 * current Cell contains exactly one non-Sequence element, the selection stays
 * on the current Cell.
 *
 * @param measure The root Measure to work with.
 * @param selected The current selection state.
 * @param amount The number of levels to move down.
 * @return The new selection path after moving down, or the original path if moving
 * down is not possible.
 */
[[nodiscard]] auto move_down(Measure const &measure, SelectedState selected,
                             std::size_t amount = 1) -> SelectedState;

} // namespace xen
