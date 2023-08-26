#pragma once

#include <optional>

#include <sequence/modify.hpp>

#include "input_mode.hpp"
#include "selection.hpp"
#include "state.hpp"
#include "timeline.hpp"

namespace xen::action
{

// These can throw exceptions with error messages and those will be displayed to the
// user

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

[[nodiscard]] inline auto cut(XenTimeline const &tl) -> std::pair<sequence::Cell, State>
{
    auto const buffer = ::xen::action::copy(tl);

    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &selected = get_selected_cell(state.phrase, aux.selected);
    selected = sequence::Rest{};

    return {buffer, state};
}

[[nodiscard]] inline auto paste(XenTimeline const &tl,
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
    // top level is not a sequence, for things like movement.
    selected = *copy_buffer;
    return state;
}

[[nodiscard]] inline auto duplicate(XenTimeline const &tl) -> std::pair<AuxState, State>
{
    auto const buffer = ::xen::action::copy(tl);
    auto const aux = ::xen::action::move_right(tl);

    // Reimplement paste here so it isn't recorded in timeline
    auto state = tl.get_state().first;
    auto &selected = get_selected_cell(state.phrase, aux.selected);
    // FIXME if the copy buffer is not a sequence and the selected cell is the top
    // level, then this will silently fail because you don't have a use case where the
    // top level is not a sequence, for things like movement.
    selected = buffer;
    return {aux, state};
}

[[nodiscard]] inline auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.input_mode = mode;
    return aux;
}

[[nodiscard]] inline auto note(XenTimeline const &tl, int interval, float velocity,
                               float delay, float gate) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::note(interval, velocity, delay, gate);
    return state;
}

[[nodiscard]] inline auto rest(XenTimeline const &tl) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::rest();
    return state;
}

[[nodiscard]] inline auto flip(XenTimeline const &tl) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::flip(cell);
    return state;
}

[[nodiscard]] inline auto split(XenTimeline const &tl, std::size_t count) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::repeat(cell, count);
    return state;
}

[[nodiscard]] inline auto extract(XenTimeline const &tl) -> std::pair<State, AuxState>
{
    auto aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    sequence::Cell *parent = get_parent_of_selected(state.phrase, aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't extract top level Cell."};
    }

    *parent = get_selected_cell(state.phrase, aux.selected);
    return {state, action::move_up(tl)};
}

[[nodiscard]] inline auto shift_note(XenTimeline const &tl, int amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::shift_pitch(cell, amount);
    return state;
}

[[nodiscard]] inline auto shift_note_octave(XenTimeline const &tl, int amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    auto const tuning_length = state.tuning.intervals.size();
    cell = sequence::modify::shift_pitch(cell, amount * (int)tuning_length);
    return state;
}

[[nodiscard]] inline auto shift_velocity(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::shift_velocity(cell, amount);
    return state;
}

[[nodiscard]] inline auto shift_delay(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::shift_delay(cell, amount);
    return state;
}

[[nodiscard]] inline auto shift_gate(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::shift_gate(cell, amount);
    return state;
}

[[nodiscard]] inline auto set_note(XenTimeline const &tl, int interval) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::set_pitch(cell, interval);
    return state;
}

[[nodiscard]] inline auto set_note_octave(XenTimeline const &tl, int octave) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto const tuning_length = state.tuning.intervals.size();
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::set_octave(cell, octave, tuning_length);
    return state;
}

[[nodiscard]] inline auto set_velocity(XenTimeline const &tl, float velocity) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::set_velocity(cell, velocity);
    return state;
}

[[nodiscard]] inline auto set_delay(XenTimeline const &tl, float delay) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::set_delay(cell, delay);
    return state;
}

[[nodiscard]] inline auto set_gate(XenTimeline const &tl, float gate) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto &cell = get_selected_cell(state.phrase, aux.selected);
    cell = sequence::modify::set_gate(cell, gate);
    return state;
}

} // namespace xen::action