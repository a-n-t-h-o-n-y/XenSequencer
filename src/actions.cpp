#include "actions.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include "serialize_state.hpp"
#include "util.hpp"

namespace xen::action
{

// These can throw exceptions with error messages and those will be displayed

auto move_left(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    if (!phrase.empty())
    {
        aux.selected = move_left(phrase, aux.selected);
    }

    return aux;
}

auto move_right(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    if (!phrase.empty())
    {
        aux.selected = move_right(phrase, aux.selected);
    }
    return aux;
}

auto move_up(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.selected = xen::move_up(aux.selected);
    return aux;
}

auto move_down(XenTimeline const &tl) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    aux.selected = xen::move_down(phrase, aux.selected);
    return aux;
}

auto copy(XenTimeline const &tl) -> std::optional<sequence::Cell>
{
    auto const aux = tl.get_aux_state();
    auto const state = tl.get_state().first;
    auto const *selected = get_selected_cell_const(state.phrase, aux.selected);
    if (selected == nullptr)
    {
        return std::nullopt;
    }
    return *selected;
}

auto cut(XenTimeline const &tl) -> std::optional<std::pair<sequence::Cell, State>>
{
    auto const buffer = ::xen::action::copy(tl);

    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    if (selected == nullptr || buffer == std::nullopt)
    {
        return std::nullopt;
    }
    *selected = sequence::Rest{};

    return std::pair{*buffer, state};
}

auto paste(XenTimeline const &tl, std::optional<sequence::Cell> const &copy_buffer)
    -> std::optional<State>
{
    if (!copy_buffer.has_value())
    {
        return std::nullopt;
    }
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    // FIXME if the copy buffer is not a sequence and the selected cell is the top
    // level, then this will silently fail because you don't have a use case where the
    // top level is not a sequence, for things like movement.
    if (selected)
    {
        *selected = *copy_buffer;
    }
    return state;
}

auto duplicate(XenTimeline const &tl) -> std::pair<AuxState, State>
{
    auto const buffer = ::xen::action::copy(tl);
    auto const aux = ::xen::action::move_right(tl);

    // Reimplement paste here so it isn't recorded in timeline
    auto state = tl.get_state().first;
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    // FIXME if the copy buffer is not a sequence and the selected cell is the top
    // level, then this will silently fail because you don't have a use case where the
    // top level is not a sequence, for things like movement.
    if (selected && buffer.has_value())
    {
        *selected = *buffer;
    }
    return {aux, state};
}

auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.input_mode = mode;
    return aux;
}

auto note(XenTimeline const &tl, int interval, float velocity, float delay, float gate)
    -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::note(interval, velocity, delay, gate);
    }
    return state;
}

auto rest(XenTimeline const &tl) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::rest();
    }
    return state;
}

auto flip(XenTimeline const &tl) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::flip(*cell);
    }
    return state;
}

auto split(XenTimeline const &tl, std::size_t count) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::repeat(*cell, count);
    }
    return state;
}

auto extract(XenTimeline const &tl) -> std::pair<State, AuxState>
{
    // TODO
    // FIXME Extracting first cell in a sequence gives an empty sequence or cell?
    auto aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    sequence::Cell *parent = get_parent_of_selected(state.phrase, aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't extract top level Cell."};
    }

    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        // Take copy because you are writing over the owner of cell.
        auto cell_copy = std::move(*cell);
        *parent = std::move(cell_copy);
    }
    return {state, action::move_up(tl)};
}

auto shift_note(XenTimeline const &tl, int amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::shift_pitch(*cell, amount);
    }
    return state;
}

auto shift_note_octave(XenTimeline const &tl, int amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        auto const tuning_length = state.tuning.intervals.size();
        *cell = sequence::modify::shift_pitch(*cell, amount * (int)tuning_length);
    }
    return state;
}

auto shift_velocity(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::shift_velocity(*cell, amount);
    }
    return state;
}

auto shift_delay(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::shift_delay(*cell, amount);
    }
    return state;
}

auto shift_gate(XenTimeline const &tl, float amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::shift_gate(*cell, amount);
    }
    return state;
}

auto set_note(XenTimeline const &tl, int interval) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_pitch(*cell, interval);
    }
    return state;
}

auto set_note_octave(XenTimeline const &tl, int octave) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto const tuning_length = state.tuning.intervals.size();
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_octave(*cell, octave, tuning_length);
    }
    return state;
}

auto set_velocity(XenTimeline const &tl, float velocity) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_velocity(*cell, velocity);
    }
    return state;
}

auto set_delay(XenTimeline const &tl, float delay) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_delay(*cell, delay);
    }
    return state;
}

auto set_gate(XenTimeline const &tl, float gate) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_gate(*cell, gate);
    }
    return state;
}

auto add_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>
{
    auto state = tl.get_state().first;
    state.phrase.push_back({sequence::Sequence{{sequence::Rest{}}}, ts});
    return {{{state.phrase.size() - 1, {}}}, state};
}

auto delete_cell(AuxState aux, State state) -> std::pair<AuxState, State>
{
    // delete selected cell, if the selected cell is the top level then delete the
    // measure from phrase
    if (state.phrase.empty())
    {
        return {aux, state};
    }

    sequence::Cell *parent = get_parent_of_selected(state.phrase, aux.selected);
    if (parent == nullptr)
    {
        // Delete Measure
        state.phrase.erase(std::next(
            std::begin(state.phrase),
            (std::vector<sequence::Measure>::difference_type)aux.selected.measure));

        aux.selected.measure = std::min(aux.selected.measure, state.phrase.size() - 1);
        aux.selected.cell.clear();
    }
    else
    {
        // Delete Cell
        if (!std::holds_alternative<sequence::Sequence>(*parent))
        {
            throw std::logic_error{"A parent Cell must be a Sequence."};
        }
        auto &cells = std::get<sequence::Sequence>(*parent).cells;
        cells.erase(std::next(
            std::begin(cells),
            (std::vector<sequence::Cell>::difference_type)aux.selected.cell.back()));

        if (cells.empty())
        {
            aux.selected = xen::move_up(aux.selected);
            return delete_cell(aux, state);
        }
        else
        {
            aux.selected.cell.back() =
                std::min(aux.selected.cell.back(), cells.size() - 1);
        }
    }

    return {aux, state};
}

auto save_state(XenTimeline const &tl, std::string const &filepath) -> void
{
    auto const json_str = serialize(tl.get_state().first);
    write_string_to_file(filepath, json_str);
}

auto load_state(std::string const &filepath) -> State
{
    auto const json_str = read_file_to_string(filepath);
    return deserialize(json_str);
}

} // namespace xen::action