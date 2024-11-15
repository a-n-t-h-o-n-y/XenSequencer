#include <xen/actions.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
#include <sequence/utility.hpp>

#include <xen/copy_paste.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace xen::action
{

// These can throw exceptions with error messages and those will be displayed as errors
// in the status bar.

auto move_left(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto [state, aux] = tl.get_state();
    aux.selected = move_left(state.sequence_bank, aux.selected, amount);
    return aux;
}

auto move_right(XenTimeline const &tl, std::size_t amount) -> AuxState
{
    auto [state, aux] = tl.get_state();
    aux.selected = move_right(state.sequence_bank, aux.selected, amount);
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
    aux.selected = xen::move_down(state.sequence_bank, aux.selected, amount);
    return aux;
}

void copy(XenTimeline const &tl)
{
    auto const [state, aux] = tl.get_state();
    write_copy_buffer(get_selected_cell_const(state.sequence_bank, aux.selected));
}

auto cut(XenTimeline const &tl) -> SequencerState
{
    ::xen::action::copy(tl);

    auto [state, aux] = tl.get_state();
    auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
    selected = sequence::Rest{};
    return state;
}

auto paste(XenTimeline const &tl) -> SequencerState
{
    auto const cell = read_copy_buffer();

    if (!cell.has_value())
    {
        throw std::runtime_error{"Copy Buffer Is Empty"};
    }

    auto [state, aux] = tl.get_state();
    auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
    selected = *cell;
    return state;
}

auto duplicate(XenTimeline const &tl) -> TrackedState
{
    auto [state, aux] = tl.get_state();
    auto selected_copy = get_selected_cell(state.sequence_bank, aux.selected);

    auto new_selection = ::xen::move_right(state.sequence_bank, aux.selected, 1);
    auto &selected = get_selected_cell(state.sequence_bank, new_selection);
    selected = selected_copy;
    aux.selected = new_selection;

    return {state, aux};
}

auto set_input_mode(XenTimeline const &tl, InputMode mode) -> AuxState
{
    auto [_, aux] = tl.get_state();
    aux.input_mode = mode;
    return aux;
}

auto lift(XenTimeline const &tl) -> TrackedState
{
    auto [state, aux] = tl.get_state();
    sequence::Cell *parent = get_parent_of_selected(state.sequence_bank, aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't lift top level Cell."};
    }

    auto &cell = get_selected_cell(state.sequence_bank, aux.selected);

    // Move out Cell because you are writing over the owner of cell.
    // Do not get rid of this local variable.
    auto cell_copy = std::move(cell);
    *parent = std::move(cell_copy);

    return {state, action::move_up(tl, 1)};
}

auto shift_octave(XenTimeline const &tl, sequence::Pattern const &pattern, int amount)
    -> SequencerState
{
    auto [state, aux] = tl.get_state();
    auto &cell = get_selected_cell(state.sequence_bank, aux.selected);
    auto const tuning_length = state.tuning.intervals.size();
    cell = sequence::modify::shift_pitch(cell, pattern, amount * (int)tuning_length);
    return state;
}

auto set_note_octave(XenTimeline const &tl, sequence::Pattern const &pattern,
                     int octave) -> SequencerState
{
    auto [state, aux] = tl.get_state();
    auto const tuning_length = state.tuning.intervals.size();
    auto &cell = get_selected_cell(state.sequence_bank, aux.selected);
    cell = sequence::modify::set_octave(cell, pattern, octave, tuning_length);
    return state;
}

auto delete_cell(TrackedState ts) -> TrackedState
{
    // Delete selected cell.
    // If the selected cell is the top level then replace it with a Rest.

    sequence::Cell *parent =
        get_parent_of_selected(ts.sequencer.sequence_bank, ts.aux.selected);
    if (parent != nullptr)
    {
        // Delete Cell
        assert(std::holds_alternative<sequence::Sequence>(*parent));
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
    else // Replace with a rest
    {

        ts.sequencer.sequence_bank[ts.aux.selected.measure].cell = sequence::Rest{};
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
    if (std::filesystem::file_size(filepath) > (128 * 1'024 * 1'024))
    {
        throw std::runtime_error{"Measure file size exceeds 128MB"};
    }

    auto const json_str = read_file_to_string(filepath);
    return deserialize_measure(json_str);
}

auto set_base_frequency(XenTimeline const &tl, float freq) -> SequencerState
{
    auto [state, _] = tl.get_state();
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
}

auto set_selected_sequence(AuxState aux, int index) -> AuxState
{

    if (index < 0 || index > 15)
    {
        throw std::runtime_error{
            "Invalid Sequence Index; Must be in closed range [0, 15]: " +
            std::to_string(index) + " was given."};
    }
    aux.selected.measure = (std::size_t)index;

    // TODO implement stored cell selection vectors in array of 16 and restore from it.
    aux.selected.cell.clear();

    return aux;
}

auto shift_scale_mode(Scale scale, int amount) -> Scale
{
    auto const size = (int)scale.intervals.size();
    auto const offset = (scale.mode - 1 + amount) % size;
    scale.mode = (std::uint8_t)((offset >= 0) ? offset + 1 : offset + size + 1);
    return scale;
}

auto shift_scale_index(std::optional<std::size_t> current, int shift_amount,
                       std::size_t scale_count) -> std::optional<std::size_t>
{
    if (scale_count == 0)
    {
        return std::nullopt;
    }

    if (shift_amount == 0)
    {
        return current;
    }

    if (current == std::nullopt)
    {
        current = 0;
        shift_amount -= 1;
    }

    auto offset = ((int)*current + shift_amount) % ((int)scale_count + 1);
    if (offset < 0)
    {
        offset = offset + (int)scale_count + 1;
    }

    if (offset == (int)scale_count) // chromatic
    {
        return std::nullopt;
    }
    else
    {
        return offset;
    }
}

auto step(sequence::Cell cell, std::size_t count, int pitch_distance,
          float velocity_distance) -> sequence::Cell
{
    if (velocity_distance > 1.f || velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    // Split
    cell = sequence::modify::repeat(cell, count);

    assert(std::holds_alternative<sequence::Sequence>(cell));

    auto &seq = std::get<sequence::Sequence>(cell);

    // Increment Pitch
    for (auto i = (std::size_t)0; i < seq.cells.size(); ++i)
    {
        seq.cells[i] = sequence::modify::shift_pitch(seq.cells[i], {0, {1}},
                                                     (int)i * pitch_distance);
    }

    // Increment Velocity
    for (auto i = (std::size_t)0; i < seq.cells.size(); ++i)
    {
        seq.cells[i] = sequence::modify::shift_velocity(seq.cells[i], {0, {1}},
                                                        (float)i * velocity_distance);
    }

    return cell;
}

auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::Cell
{
    return std::visit(
        sequence::utility::overload{
            [](sequence::Note const &note) -> sequence::Cell { return note; },
            [](sequence::Rest const &rest) -> sequence::Cell { return rest; },
            [&](sequence::Sequence &seq) -> sequence::Cell {
                auto view = sequence::PatternView{seq.cells, pattern};
                auto i = std::size_t{0};
                for (auto &c : view)
                {
                    c = sequence::modify::shift_pitch(c, {0, {1}},
                                                      intervals[i % intervals.size()]);
                    ++i;
                }
                return seq;
            },
        },
        cell);
}

} // namespace xen::action
