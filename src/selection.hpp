#pragma once

#include <cstddef>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>

#include "state.hpp"

namespace xen
{

/**
 * @brief Returns a reference to the selected Cell based on the given index vector.
 *
 * @param phrase The Phrase to select from.
 * @param indices A vector containing the indices to navigate through the nested
 * structure to the selected cell.
 * @return Cell* Pointer to the selected Cell.
 *
 * @exception std::runtime_error If an index is out of bounds or if an invalid index
 * is provided for a non-Sequence Cell.
 */
[[nodiscard]] auto get_selected_cell(sequence::Phrase &phrase,
                                     SelectedState const &selected) -> sequence::Cell *;

/**
 * @brief Returns a reference to the selected Cell based on the given index vector.
 *
 * @param phrase The Phrase to select from.
 * @param indices A vector containing the indices to navigate through the nested
 * structure to the selected cell.
 * @return Cell const* Const Pointer to the selected Cell.
 *
 * @exception std::runtime_error If an index is out of bounds or if an invalid index
 * is provided for a non-Sequence Cell.
 */
[[nodiscard]] auto get_selected_cell_const(sequence::Phrase const &phrase,
                                           SelectedState const &selected)
    -> sequence::Cell const *;

/**
 * Utility to get the parent Sequence of the currently selected Cell.
 *
 * @param head The root sequence.
 * @param selected The current selection indices.
 * @return Pointer to the parent of the selected Cell, or nullptr if the selection
 * is at the top level.
 *
 * @throws std::runtime_error If any index is out of bounds.
 * @throws std::bad_variant_access If parent is not a Sequence.
 */
[[nodiscard]] auto get_parent_of_selected(sequence::Phrase &phrase,
                                          SelectedState const &selected)
    -> sequence::Cell *;

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
[[nodiscard]] auto get_parent_of_selected_const(sequence::Phrase const &phrase,
                                                SelectedState const &selected)
    -> sequence::Cell const *;

/**
 * Utility to get the number of siblings of the currently selected Cell.
 *
 * This count is the total number of child cells of the selected cell's parent.
 * This includes the selected cell itself.
 *
 * @param phrase The root sequence.
 * @param selected The current selection indices.
 * @return The number of siblings of the selected Cell.
 */
[[nodiscard]] auto get_sibling_count(sequence::Phrase const &phrase,
                                     SelectedState const &selected) -> std::size_t;

/**
 * Move the selection left within the current sequence.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving left.
 */
[[nodiscard]] auto move_left(sequence::Phrase const &phrase, SelectedState selected)
    -> SelectedState;

/**
 * Move the selection right within the current sequence.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving right.
 */
[[nodiscard]] auto move_right(sequence::Phrase const &phrase, SelectedState selected)
    -> SelectedState;

/**
 * Move the selection up in the sequence hierarchy if possible.
 *
 * @param selected The current selection indices.
 * @return The new selection indices after moving up.
 */
[[nodiscard]] auto move_up(SelectedState selected) -> SelectedState;

/**
 * Move the selection down in the sequence hierarchy if possible.
 *
 * @param phrase The root Phrase.
 * @param selected The current selection indices.
 * @return The new selection indices after moving down, or the original indices if
 * moving down is not possible.
 */
[[nodiscard]] auto move_down(sequence::Phrase const &phrase, SelectedState selected)
    -> SelectedState;

} // namespace xen