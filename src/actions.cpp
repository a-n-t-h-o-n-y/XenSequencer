#include <xen/actions.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sequence/modify.hpp>
#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace
{

[[nodiscard]] auto paste_logic(xen::State &state, xen::AuxState const &aux,
                               std::optional<sequence::Cell> const &copy_buffer)
    -> xen::State
{
    if (!copy_buffer.has_value())
    {
        throw std::runtime_error{"Copy Buffer Is Empty"};
    }
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    if (!selected)
    {
        throw std::runtime_error{"No Selection"};
    }
    *selected = *copy_buffer;
    return state;
}

} // namespace

namespace xen::action
{

// These can throw exceptions with error messages and those will be displayed

auto move_left(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    if (!phrase.empty())
    {
        aux.selected = move_left(phrase, aux.selected, amount);
    }

    return aux;
}

auto move_right(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    if (!phrase.empty())
    {
        aux.selected = move_right(phrase, aux.selected, amount);
    }
    return aux;
}

auto move_up(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.selected = xen::move_up(aux.selected, amount);
    return aux;
}

auto move_down(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto aux = tl.get_aux_state();
    auto const phrase = tl.get_state().first.phrase;
    aux.selected = xen::move_down(phrase, aux.selected, amount);
    return aux;
}

auto copy(XenTimeline const &tl) -> sequence::Cell
{
    auto const aux = tl.get_aux_state();
    auto const state = tl.get_state().first;
    auto const *selected = get_selected_cell_const(state.phrase, aux.selected);
    if (selected == nullptr)
    {
        throw std::runtime_error{"No Selection"};
    }
    return *selected;
}

auto cut(XenTimeline const &tl) -> std::pair<sequence::Cell, State>
{
    auto const buffer = ::xen::action::copy(tl);

    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    if (selected == nullptr)
    {
        throw std::runtime_error{"No Selection"};
    }
    *selected = sequence::Rest{};
    return {buffer, state};
}

auto paste(XenTimeline const &tl, std::optional<sequence::Cell> const &copy_buffer)
    -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    return paste_logic(state, aux, copy_buffer);
}

auto duplicate(XenTimeline const &tl) -> std::pair<AuxState, State>
{
    auto const buffer = ::xen::action::copy(tl);
    auto const aux = ::xen::action::move_right(tl, 1);
    auto state = tl.get_state().first;
    return {aux, ::paste_logic(state, aux, buffer)};
}

auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState
{
    auto aux = tl.get_aux_state();
    aux.input_mode = mode;
    return aux;
}

auto lift(XenTimeline const &tl) -> std::pair<State, AuxState>
{
    auto aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    sequence::Cell *parent = get_parent_of_selected(state.phrase, aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't lift top level Cell."};
    }

    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        // Take copy because you are writing over the owner of cell.
        auto cell_copy = std::move(*cell);
        *parent = std::move(cell_copy);
    }
    return {state, action::move_up(tl, 1)};
}

auto shift_note_octave(XenTimeline const &tl, sequence::Pattern const &pattern,
                       int amount) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        auto const tuning_length = state.tuning.intervals.size();
        *cell = sequence::modify::shift_interval(*cell, pattern,
                                                 amount * (int)tuning_length);
    }
    return state;
}

auto set_note_octave(XenTimeline const &tl, sequence::Pattern const &pattern,
                     int octave) -> State
{
    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto const tuning_length = state.tuning.intervals.size();
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_octave(*cell, pattern, octave, tuning_length);
    }
    return state;
}

auto append_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>
{
    auto state = tl.get_state().first;
    state.phrase.push_back(sequence::Measure{sequence::Rest{}, ts});
    return {{{state.phrase.size() - 1, {}}}, state};
}

auto insert_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>
{
    auto state = tl.get_state().first;
    auto const current_measure = tl.get_aux_state().selected.measure;
    state.phrase.insert(
        std::next(std::begin(state.phrase), (std::ptrdiff_t)current_measure),
        sequence::Measure{sequence::Rest{}, ts});
    return {{{current_measure, {}}}, state};
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
            aux.selected = xen::move_up(aux.selected, 1);
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

auto save_state(XenTimeline const &tl, std::filesystem::path const &filepath) -> void
{
    auto const json_str = serialize_state(tl.get_state().first);
    write_string_to_file(filepath, json_str);
}

auto load_state(std::filesystem::path const &filepath) -> State
{
    auto const json_str = read_file_to_string(filepath);
    return deserialize_state(json_str);
}

auto set_timesignature(XenTimeline const &tl, sequence::TimeSignature ts) -> State
{
    auto const measure_index = tl.get_aux_state().selected.measure;
    auto state = tl.get_state().first;
    auto &measure = state.phrase[measure_index];
    measure.time_signature = ts;
    return state;
}

auto set_base_frequency(XenTimeline const &tl, float freq) -> State
{
    auto state = tl.get_state().first;
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
}

} // namespace xen::action
