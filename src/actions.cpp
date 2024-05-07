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

[[nodiscard]] auto paste_logic(xen::SequencerState &state, xen::AuxState const &aux,
                               std::optional<sequence::Cell> const &copy_buffer)
    -> xen::SequencerState
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
    auto [state, aux] = tl.get_state();
    if (!state.phrase.empty())
    {
        aux.selected = move_left(state.phrase, aux.selected, amount);
    }
    return aux;
}

auto move_right(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto [state, aux] = tl.get_state();
    if (!state.phrase.empty())
    {
        aux.selected = move_right(state.phrase, aux.selected, amount);
    }
    return aux;
}

auto move_up(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto [_, aux] = tl.get_state();
    aux.selected = xen::move_up(aux.selected, amount);
    return aux;
}

auto move_down(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto [state, aux] = tl.get_state();
    aux.selected = xen::move_down(state.phrase, aux.selected, amount);
    return aux;
}

auto copy(XenTimeline const &tl) -> sequence::Cell
{
    auto const [state, aux] = tl.get_state();
    auto const *selected = get_selected_cell_const(state.phrase, aux.selected);
    if (selected == nullptr)
    {
        throw std::runtime_error{"No Selection"};
    }
    return *selected;
}

auto cut(XenTimeline const &tl) -> std::pair<sequence::Cell, SequencerState>
{
    auto const buffer = ::xen::action::copy(tl);

    auto [state, aux] = tl.get_state();
    auto *selected = get_selected_cell(state.phrase, aux.selected);
    if (selected == nullptr)
    {
        throw std::runtime_error{"No Selection"};
    }
    *selected = sequence::Rest{};
    return {buffer, state};
}

auto paste(XenTimeline const &tl,
           std::optional<sequence::Cell> const &copy_buffer) -> SequencerState
{
    auto [state, aux] = tl.get_state();
    return paste_logic(state, aux, copy_buffer);
}

auto duplicate(XenTimeline const &tl) -> TrackedState
{
    auto const buffer = ::xen::action::copy(tl);
    auto aux = ::xen::action::move_right(tl, 1);
    auto [state, _] = tl.get_state();
    return {::paste_logic(state, aux, buffer), std::move(aux)};
}

auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState
{
    auto [_, aux] = tl.get_state();
    aux.input_mode = mode;
    return aux;
}

auto lift(XenTimeline const &tl) -> TrackedState
{
    auto [state, aux] = tl.get_state();
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
                       int amount) -> SequencerState
{
    auto [state, aux] = tl.get_state();
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
                     int octave) -> SequencerState
{
    auto [state, aux] = tl.get_state();
    auto const tuning_length = state.tuning.intervals.size();
    auto *cell = get_selected_cell(state.phrase, aux.selected);
    if (cell)
    {
        *cell = sequence::modify::set_octave(*cell, pattern, octave, tuning_length);
    }
    return state;
}

auto delete_cell(TrackedState ts) -> TrackedState
{
    // delete selected cell, if the selected cell is the top level then delete the
    // measure from phrase
    if (ts.sequencer.phrase.empty())
    {
        return ts;
    }

    sequence::Cell *parent =
        get_parent_of_selected(ts.sequencer.phrase, ts.aux.selected);
    if (parent == nullptr)
    {
        // Delete Measure
        ts.sequencer.phrase.erase(std::next(
            std::begin(ts.sequencer.phrase),
            (std::vector<sequence::Measure>::difference_type)ts.aux.selected.measure));

        ts.aux.selected.measure =
            std::min(ts.aux.selected.measure, ts.sequencer.phrase.size() - 1);
        ts.aux.selected.cell.clear();
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
            (std::vector<sequence::Cell>::difference_type)ts.aux.selected.cell.back()));

        if (cells.empty())
        {
            ts.aux.selected = xen::move_up(ts.aux.selected, 1);
            return delete_cell(ts);
        }
        else
        {
            ts.aux.selected.cell.back() =
                std::min(ts.aux.selected.cell.back(), cells.size() - 1);
        }
    }

    return ts;
}

auto save_measure(sequence::Measure const &measure,
                  std::filesystem::path const &filepath) -> void
{
    auto const json_str = serialize_measure(measure);
    write_string_to_file(filepath, json_str);
}

auto load_measure(std::filesystem::path const &filepath) -> sequence::Measure
{
    auto const json_str = read_file_to_string(filepath);
    return deserialize_measure(json_str);
}

auto set_base_frequency(XenTimeline const &tl, float freq) -> SequencerState
{
    auto [state, _] = tl.get_state();
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
}

} // namespace xen::action
