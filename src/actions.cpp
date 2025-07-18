#include <xen/actions.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>

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
    selected = {.element = sequence::Rest{}, .weight = selected.weight};
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
        assert(std::holds_alternative<sequence::Sequence>(parent->element));
        auto &cells = std::get<sequence::Sequence>(parent->element).cells;
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

        ts.sequencer.sequence_bank[ts.aux.selected.measure].cell = {sequence::Rest{}};
    }

    return ts;
}

auto save_measure(juce::File const &filepath, sequence::Measure const &measure) -> void
{
    filepath.replaceWithText(serialize_measure(measure));
}

auto load_measure(juce::File const &filepath) -> sequence::Measure
{
    if (filepath.getSize() > (128 * 1'024 * 1'024))
    {
        throw std::runtime_error{"Measure file size exceeds 128MB"};
    }
    return deserialize_measure(filepath.loadFileAsString().toStdString());
}

auto save_sequence_bank(SequenceBank const &bank,
                        std::array<std::string, 16> const &sequence_names,
                        juce::File const &filepath) -> void
{
    filepath.replaceWithText(serialize_sequence_bank(bank, sequence_names));
}

auto load_sequence_bank(juce::File const &filepath)
    -> std::pair<SequenceBank, std::array<std::string, 16>>
{
    if (filepath.getSize() > (128 * 1'024 * 1'024))
    {
        throw std::runtime_error{"Sequence Bank file size exceeds 128MB"};
    }
    return deserialize_sequence_bank(filepath.loadFileAsString().toStdString());
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

auto step(sequence::Cell cell, sequence::Pattern const &pattern, int pitch_distance,
          float velocity_distance) -> sequence::Cell
{
    if (velocity_distance > 1.f || velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        // Call shift_pitch for each cell instead of passing in sequence because each
        // cell is a different pitch and velocity value.
        auto view = sequence::PatternView{seq.cells, pattern};
        auto i = std::size_t{0};
        for (auto &c : view)
        {
            c = sequence::modify::shift_pitch(c, {0, {1}}, (int)i * pitch_distance);
            c = sequence::modify::shift_velocity(c, {0, {1}},
                                                 (float)i * velocity_distance);
            ++i;
        }
    }

    return cell;
}

auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::Cell
{
    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);
        auto view = sequence::PatternView{seq.cells, pattern};
        auto i = std::size_t{0};
        for (auto &c : view)
        {
            c = sequence::modify::shift_pitch(c, {0, {1}},
                                              intervals[i % intervals.size()]);
            ++i;
        }
    }
    return cell;
}

auto set_weight(sequence::Cell cell, float weight) -> sequence::Cell
{
    if (weight <= 0.f)
    {
        throw std::runtime_error{"Weight must be greater than 0."};
    }

    cell.weight = weight;
    return cell;
}

auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell
{
    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        for (auto i = std::size_t{0}; i < seq.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &c = seq.cells[i];
                c.weight = mod((float)i / (float)seq.cells.size());
            }
        }
    }
    return cell;
}

auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern, float weight)
    -> sequence::Cell
{
    if (weight <= 0.f)
    {
        throw std::runtime_error{"Weight must be greater than 0."};
    }

    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        auto view = sequence::PatternView{seq.cells, pattern};

        for (auto &c : view)
        {
            c.weight = weight;
        }
    }
    return cell;
}

auto set_velocities(sequence::Cell cell, sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::Cell
{
    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        for (auto i = std::size_t{0}; i < seq.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &c = seq.cells[i];
                c = sequence::modify::set_velocity(
                    c, pattern, mod((float)i / (float)seq.cells.size()));
            }
        }
    }
    return cell;
}

auto set_delays(sequence::Cell cell, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::Cell
{
    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        for (auto i = std::size_t{0}; i < seq.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &c = seq.cells[i];
                c = sequence::modify::set_delay(
                    c, pattern, mod((float)i / (float)seq.cells.size()));
            }
        }
    }
    return cell;
}

auto set_gates(sequence::Cell cell, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::Cell
{
    if (std::holds_alternative<sequence::Sequence>(cell.element))
    {
        auto &seq = std::get<sequence::Sequence>(cell.element);

        for (auto i = std::size_t{0}; i < seq.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &c = seq.cells[i];
                c = sequence::modify::set_gate(c, pattern,
                                               mod((float)i / (float)seq.cells.size()));
            }
        }
    }
    return cell;
}

} // namespace xen::action
