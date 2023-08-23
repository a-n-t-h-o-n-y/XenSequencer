#pragma once

#include <optional>

#include "selection.hpp"
#include "state.hpp"
#include "timeline.hpp"

namespace xen::action
{

[[nodiscard]] inline auto move_left(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    aux.selected = move_left(phrase, aux.selected);
    return aux;
}

[[nodiscard]] inline auto move_right(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    aux.selected = move_right(phrase, aux.selected);
    return aux;
}

[[nodiscard]] inline auto move_up(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.selected = xen::move_up(aux.selected);
    return aux;
}

[[nodiscard]] inline auto move_down(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    aux.selected = xen::move_down(phrase, aux.selected);
    return aux;
}

[[nodiscard]] inline auto copy(XenTimeline const &tl) -> sequence::Cell
{
    auto const aux = tl.get_aux_state();
    auto const state = tl.get_state().first;
    return get_selected_cell_const(state.phrase, aux.selected);
}

[[nodiscard]] inline auto cut(XenTimeline &tl) -> std::pair<sequence::Cell, State>
{
    auto const buffer = ::xen::action::copy(tl);

    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &selected = get_selected_cell(state.phrase, aux.selected);
    selected = sequence::Rest{};

    return {buffer, state};
}

[[nodiscard]] inline auto paste(XenTimeline &tl,
                                std::optional<sequence::Cell> const &copy_buffer)
    -> std::optional<State>
{
    if (!copy_buffer.has_value())
    {
        return std::nullopt;
    }
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &selected = get_selected_cell(state.phrase, aux.selected);
    // FIXME if the copy buffer is not a sequence and the selected cell is the top
    // level, then this will silently fail because you don't have a use case where the
    // top level is not a sequence.
    selected = *copy_buffer;
    return state;
}

[[nodiscard]] inline auto duplicate(XenTimeline &tl) -> std::pair<AuxState, State>
{
    auto const buffer = ::xen::action::copy(tl);
    auto const aux = ::xen::action::move_right(tl);

    // Reimplement paste here so it isn't recorded in timeline
    auto state = tl.get_state().first;
    auto &selected = get_selected_cell(state.phrase, aux.selected);
    // FIXME if the copy buffer is not a sequence and the selected cell is the top
    // level, then this will silently fail because you don't have a use case where the
    // top level is not a sequence.
    selected = buffer;
    return {aux, state};
}

} // namespace xen::action