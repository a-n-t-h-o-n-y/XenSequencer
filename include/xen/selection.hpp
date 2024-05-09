#pragma once

#include <cstddef>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * Returns a reference to the selected Cell based on the given index vector.
 *
 * @param bank The SequenceBank to select from.
 * @param selected The current selection state.
 * @return Cell& to the selected Cell.
 * @exception std::runtime_error If selected cell does not exist.
 */
[[nodiscard]] auto get_selected_cell(SequenceBank &bank,
                                     SelectedState const &selected) -> sequence::Cell &;

/**
 * Returns a reference to the selected Cell based on the given index vector.
 *
 * @param bank The SequenceBank to select from.
 * @param selected The current selection state.
 * @return Cell const& to the selected Cell.
 * @exception std::runtime_error If selected cell does not exist.
 */
[[nodiscard]] auto get_selected_cell_const(
    SequenceBank const &bank, SelectedState const &selected) -> sequence::Cell const &;

/**
 * Utility to get the parent Sequence of the currently selected Cell.
 *
 * @param bank The SequenceBank to select from.
 * @param selected The current selection state.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection
 * is at the top level.
 * @throws std::bad_variant_access If parent is not a Sequence.
 */
[[nodiscard]] auto get_parent_of_selected(
    SequenceBank &bank, SelectedState const &selected) -> sequence::Cell *;

/**
 * Utility to get the parent Sequence of the currently selected Cell.
 *
 * @param bank The SequenceBank to select from.
 * @param selected The current selection state.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection is
 * at the top level.
 * @throws std::bad_variant_access If parent is not a Sequence.
 */
[[nodiscard]] auto get_parent_of_selected_const(
    SequenceBank const &bank, SelectedState const &selected) -> sequence::Cell const *;

/**
 * Utility to get the number of siblings of the currently selected Cell.
 *
 * @details This count is the total number of child cells of the selected cell's parent.
 * This includes the selected cell itself.
 * @param bank The SequenceBank to select from.
 * @param selected The current selection state.
 * @return The number of siblings of the selected Cell.
 */
[[nodiscard]] auto get_sibling_count(SequenceBank const &bank,
                                     SelectedState const &selected) -> std::size_t;

/**
 * Move the selection left within the current sequence.
 *
 * @param bank The SequenceBank to work with.
 * @param selected The current selection state.
 * @param amount The number of cells to move left.
 * @return The new selection indices after moving left.
 */
[[nodiscard]] auto move_left(SequenceBank const &bank, SelectedState selected,
                             std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection right within the current sequence.
 *
 * @param bank The SequenceBank to work with.
 * @param selected The current selection state.
 * @param amount The number of cells to move right.
 * @return The new selection indices after moving right.
 */
[[nodiscard]] auto move_right(SequenceBank const &bank, SelectedState selected,
                              std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection up in the sequence hierarchy if possible.
 *
 * @param selected The current selection state.
 * @param amount The number of cells to move up.
 * @return The new selection indices after moving up, or the original indices if
 * moving up is not possible.
 */
[[nodiscard]] auto move_up(SelectedState selected,
                           std::size_t amount = 1) -> SelectedState;

/**
 * Move the selection down in the sequence hierarchy if possible.
 *
 * @param bank The SequenceBank to work with.
 * @param selected The current selection state.
 * @param amount The number of cells to move down.
 * @return The new selection indices after moving down, or the original indices if
 * moving down is not possible.
 */
[[nodiscard]] auto move_down(SequenceBank const &bank, SelectedState selected,
                             std::size_t amount = 1) -> SelectedState;

} // namespace xen