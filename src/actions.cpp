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

auto move_left(EngineState const &state, ExecutionContext context, std::size_t amount)
    -> ExecutionContext
{
    context.selected = move_left(state.sequence_bank, context.selected, amount);
    return context;
}

auto move_right(EngineState const &state, ExecutionContext context,
                std::size_t amount) -> ExecutionContext
{
    context.selected = move_right(state.sequence_bank, context.selected, amount);
    return context;
}

auto move_up(ExecutionContext context, std::size_t amount) -> ExecutionContext
{
    context.selected = xen::move_up(context.selected, amount);
    return context;
}

auto move_down(EngineState const &state, ExecutionContext context,
               std::size_t amount) -> ExecutionContext
{
    context.selected = xen::move_down(state.sequence_bank, context.selected, amount);
    return context;
}

void copy(EngineState const &state, ExecutionContext const &context)
{
    write_copy_buffer(get_selected_cell_const(state.sequence_bank, context.selected));
}

auto cut(EngineState state, ExecutionContext const &context) -> EngineState
{
    ::xen::action::copy(state, context);
    auto &selected = get_selected_cell(state.sequence_bank, context.selected);
    selected = {.element = sequence::Rest{}, .weight = selected.weight};
    return state;
}

auto paste(EngineState state, ExecutionContext const &context) -> EngineState
{
    auto const cell = read_copy_buffer();

    if (!cell.has_value())
    {
        throw std::runtime_error{"Copy Buffer Is Empty"};
    }

    auto &selected = get_selected_cell(state.sequence_bank, context.selected);
    selected = *cell;
    return state;
}

auto duplicate(TimelineState state) -> TimelineState
{
    auto selected_copy = get_selected_cell(state.sequencer.sequence_bank,
                                           state.aux.selected);

    auto new_selection =
        ::xen::move_right(state.sequencer.sequence_bank, state.aux.selected, 1);
    auto &selected = get_selected_cell(state.sequencer.sequence_bank, new_selection);
    selected = selected_copy;
    state.aux.selected = new_selection;

    return state;
}

auto set_input_mode(ExecutionContext context, InputMode mode) -> ExecutionContext
{
    context.input_mode = mode;
    return context;
}

auto lift(TimelineState state) -> TimelineState
{
    sequence::Cell *parent =
        get_parent_of_selected(state.sequencer.sequence_bank, state.aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't lift top level Cell."};
    }

    auto &cell =
        get_selected_cell(state.sequencer.sequence_bank, state.aux.selected);

    // Move out Cell because you are writing over the owner of cell.
    // Do not get rid of this local variable.
    auto cell_copy = std::move(cell);
    *parent = std::move(cell_copy);

    state.aux = action::move_up(state.aux, 1);
    return state;
}

auto shift_octave(EngineState state, ExecutionContext const &context,
                  sequence::Pattern const &pattern, int amount) -> EngineState
{
    auto &cell = get_selected_cell(state.sequence_bank, context.selected);
    auto const tuning_length = state.tuning.intervals.size();
    cell = sequence::modify::shift_pitch(cell, pattern, amount * (int)tuning_length);
    return state;
}

auto set_note_octave(EngineState state, ExecutionContext const &context,
                     sequence::Pattern const &pattern, int octave) -> EngineState
{
    auto const tuning_length = state.tuning.intervals.size();
    auto &cell = get_selected_cell(state.sequence_bank, context.selected);
    cell = sequence::modify::set_octave(cell, pattern, octave, tuning_length);
    return state;
}

auto delete_cell(TimelineState ts) -> TimelineState
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

auto save_measure(juce::File const &filepath, Measure const &measure) -> void
{
    filepath.replaceWithText(serialize_measure(measure));
}

auto load_measure(juce::File const &filepath) -> Measure
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

auto set_base_frequency(EngineState state, float freq) -> EngineState
{
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
}

auto set_selected_sequence(ExecutionContext context, int index) -> ExecutionContext
{

    if (index < 0 || index > 15)
    {
        throw std::runtime_error{
            "Invalid Sequence Index; Must be in closed range [0, 15]: " +
            std::to_string(index) + " was given."};
    }
    context.selected.measure = (std::size_t)index;

    // TODO implement stored cell selection vectors in array of 16 and restore from it.
    context.selected.cell.clear();

    return context;
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

void flip_translate_direction(TranslateDirection &td)
{
    td = (td == TranslateDirection::Up) ? TranslateDirection::Down
                                        : TranslateDirection::Up;
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

auto set_pitches(sequence::Cell cell, sequence::Pattern const &pattern,
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
                c = sequence::modify::set_pitch(
                    c, pattern,
                    (int)std::floor(evaluate(mod, (float)i / (float)seq.cells.size())));
            }
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
                c.weight = evaluate(mod, (float)i / (float)seq.cells.size());
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
                    c, pattern, evaluate(mod, (float)i / (float)seq.cells.size()));
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
                    c, pattern, evaluate(mod, (float)i / (float)seq.cells.size()));
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
                c = sequence::modify::set_gate(
                    c, pattern, evaluate(mod, (float)i / (float)seq.cells.size()));
            }
        }
    }
    return cell;
}

} // namespace xen::action
