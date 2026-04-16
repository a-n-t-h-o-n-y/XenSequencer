#include <xen/actions.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
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

#include <xen/copy_paste.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace xen::action
{

namespace
{

template <typename Fn>
auto visit_sequence(sequence::MusicElement element, Fn &&fn) -> sequence::MusicElement
{
    if (auto *sequence = std::get_if<sequence::Sequence>(&element))
    {
        fn(*sequence);
    }
    return element;
}

template <typename Fn>
auto visit_sequence(sequence::Cell cell, Fn &&fn) -> sequence::Cell
{
    for (auto &element : cell.elements)
    {
        if (auto *sequence = std::get_if<sequence::Sequence>(&element))
        {
            fn(*sequence);
        }
    }
    return cell;
}

auto erase_selected_element(sequence::Cell &cell, std::size_t element_index) -> void
{
    cell.elements.erase(std::next(std::begin(cell.elements),
                                  (std::vector<sequence::MusicElement>::difference_type)
                                      element_index));
}

} // namespace

auto move_left(EngineState const &state, ExecutionContext context, std::size_t amount)
    -> ExecutionContext
{
    context.selected = xen::move_left(state.measure, context.selected, amount);
    return context;
}

auto move_right(EngineState const &state, ExecutionContext context,
                std::size_t amount) -> ExecutionContext
{
    context.selected = xen::move_right(state.measure, context.selected, amount);
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
    context.selected = xen::move_down(state.measure, context.selected, amount);
    return context;
}

void copy(EngineState const &state, ExecutionContext const &context)
{
    if (selection_kind(context.selected) == SelectionKind::Element)
    {
        write_copy_buffer(get_selected_element_const(state.measure, context.selected));
    }
    else
    {
        write_copy_buffer(get_selected_cell_const(state.measure, context.selected));
    }
}

auto paste(EngineState state, ExecutionContext const &context) -> EngineState
{
    auto const content = read_copy_buffer();

    if (!content.has_value())
    {
        throw std::runtime_error{"Copy Buffer Is Empty"};
    }

    if (std::holds_alternative<sequence::Cell>(*content))
    {
        auto replacement = std::get<sequence::Cell>(*content);
        if (selection_kind(context.selected) == SelectionKind::Element)
        {
            auto *parent_cell =
                get_parent_cell_of_selection(state.measure, context.selected);
            *parent_cell = std::move(replacement);
        }
        else
        {
            auto &selected = get_selected_cell(state.measure, context.selected);
            selected = std::move(replacement);
        }
    }
    else
    {
        auto element = std::get<sequence::MusicElement>(*content);
        if (selection_kind(context.selected) == SelectionKind::Element)
        {
            auto &parent_cell =
                *get_parent_cell_of_selection(state.measure, context.selected);
            auto const index = get_selected_element_index(context.selected);
            parent_cell.elements.insert(
                std::next(std::begin(parent_cell.elements),
                          (std::vector<sequence::MusicElement>::difference_type)
                              (index + 1)),
                std::move(element));
        }
        else
        {
            auto &selected = get_selected_cell(state.measure, context.selected);
            selected.elements.push_back(std::move(element));
        }
    }

    return state;
}

auto duplicate(TimelineState state) -> TimelineState
{
    if (selection_kind(state.aux.selected) == SelectionKind::Element)
    {
        auto &cell = get_selected_cell(state.sequencer.measure, state.aux.selected);
        auto const index = get_selected_element_index(state.aux.selected);
        auto copy = cell.elements.at(index);
        cell.elements.insert(
            std::next(std::begin(cell.elements),
                      (std::vector<sequence::MusicElement>::difference_type)(index + 1)),
            copy);
        state.aux.selected = select_element_in_cell(
            select_parent_cell(state.aux.selected), index + 1);
        return state;
    }

    auto selected_copy = get_selected_cell(state.sequencer.measure, state.aux.selected);

    auto new_selection = ::xen::move_right(state.sequencer.measure,
                                           state.aux.selected, 1);
    auto &selected = get_selected_cell(state.sequencer.measure, new_selection);
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
    if (selection_kind(state.aux.selected) == SelectionKind::Element)
    {
        auto &cell = get_selected_cell(state.sequencer.measure, state.aux.selected);
        auto element =
            std::move(cell.elements.at(get_selected_element_index(state.aux.selected)));
        cell.elements.clear();
        cell.elements.push_back(std::move(element));
        state.aux.selected = select_parent_cell(state.aux.selected);
        return state;
    }

    sequence::Cell *parent =
        get_parent_of_selected(state.sequencer.measure, state.aux.selected);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't lift top level Cell."};
    }

    auto &cell =
        get_selected_cell(state.sequencer.measure, state.aux.selected);

    auto cell_copy = std::move(cell);
    *parent = std::move(cell_copy);

    state.aux.selected = select_parent_cell(state.aux.selected);
    return state;
}

auto shift_octave(EngineState state, ExecutionContext const &context,
                  sequence::Pattern const &pattern, int amount) -> EngineState
{
    auto const tuning_length = state.tuning.intervals.size();
    if (selection_kind(context.selected) == SelectionKind::Element)
    {
        auto &element = get_selected_element(state.measure, context.selected);
        element = sequence::modify::shift_pitch(element, pattern,
                                                amount * (int)tuning_length);
    }
    else
    {
        auto &cell = get_selected_cell(state.measure, context.selected);
        cell = sequence::modify::shift_pitch(cell, pattern,
                                             amount * (int)tuning_length);
    }
    return state;
}

auto set_note_octave(EngineState state, ExecutionContext const &context,
                     sequence::Pattern const &pattern, int octave) -> EngineState
{
    auto const tuning_length = state.tuning.intervals.size();
    if (selection_kind(context.selected) == SelectionKind::Element)
    {
        auto &element = get_selected_element(state.measure, context.selected);
        element = sequence::modify::set_octave(element, pattern, octave, tuning_length);
    }
    else
    {
        auto &cell = get_selected_cell(state.measure, context.selected);
        cell = sequence::modify::set_octave(cell, pattern, octave, tuning_length);
    }
    return state;
}

auto delete_cell(TimelineState ts) -> TimelineState
{
    if (selection_kind(ts.aux.selected) == SelectionKind::Element)
    {
        auto &selected_cell =
            *get_parent_cell_of_selection(ts.sequencer.measure, ts.aux.selected);
        auto const index = get_selected_element_index(ts.aux.selected);
        erase_selected_element(selected_cell, index);

        if (selected_cell.elements.empty())
        {
            ts.aux.selected = select_parent_cell(ts.aux.selected);
        }
        else
        {
            ts.aux.selected = select_element_in_cell(
                select_parent_cell(ts.aux.selected),
                std::min(index, selected_cell.elements.size() - 1));
        }

        return ts;
    }

    auto &selected_cell = get_selected_cell(ts.sequencer.measure, ts.aux.selected);
    selected_cell.elements.clear();
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

auto set_base_frequency(EngineState state, float freq) -> EngineState
{
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
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

    if (offset == (int)scale_count)
    {
        return std::nullopt;
    }

    return offset;
}

void flip_translate_direction(TranslateDirection &td)
{
    td = (td == TranslateDirection::Up) ? TranslateDirection::Down
                                        : TranslateDirection::Up;
}

auto step(sequence::MusicElement element, sequence::Pattern const &pattern,
          int pitch_distance, float velocity_distance) -> sequence::MusicElement
{
    if (velocity_distance > 1.f || velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &cell : view)
        {
            cell = sequence::modify::shift_pitch(cell, {0, {1}},
                                                 (int)i * pitch_distance);
            cell = sequence::modify::shift_velocity(cell, {0, {1}},
                                                    (float)i * velocity_distance);
            ++i;
        }
    });
}

auto step(sequence::Cell cell, sequence::Pattern const &pattern, int pitch_distance,
          float velocity_distance) -> sequence::Cell
{
    if (velocity_distance > 1.f || velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &target_cell : view)
        {
            target_cell = sequence::modify::shift_pitch(target_cell, {0, {1}},
                                                        (int)i * pitch_distance);
            target_cell = sequence::modify::shift_velocity(
                target_cell, {0, {1}}, (float)i * velocity_distance);
            ++i;
        }
    });
}

auto arp(sequence::MusicElement element, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &cell : view)
        {
            cell = sequence::modify::shift_pitch(cell, {0, {1}},
                                                 intervals[i % intervals.size()]);
            ++i;
        }
    });
}

auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &target_cell : view)
        {
            target_cell = sequence::modify::shift_pitch(
                target_cell, {0, {1}}, intervals[i % intervals.size()]);
            ++i;
        }
    });
}

auto set_pitches(sequence::MusicElement element, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell = sequence::modify::set_pitch(
                    cell, pattern,
                    (int)std::floor(evaluate(mod, (float)i / (float)sequence.cells.size())));
            }
        }
    });
}

auto set_pitches(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &target_cell = sequence.cells[i];
                target_cell = sequence::modify::set_pitch(
                    target_cell, pattern,
                    (int)std::floor(evaluate(mod, (float)i / (float)sequence.cells.size())));
            }
        }
    });
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

auto set_weights(sequence::MusicElement element, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell.weight = evaluate(mod, (float)i / (float)sequence.cells.size());
            }
        }
    });
}

auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &target_cell = sequence.cells[i];
                target_cell.weight =
                    evaluate(mod, (float)i / (float)sequence.cells.size());
            }
        }
    });
}

auto set_weights(sequence::MusicElement element, sequence::Pattern const &pattern,
                 float weight) -> sequence::MusicElement
{
    if (weight <= 0.f)
    {
        throw std::runtime_error{"Weight must be greater than 0."};
    }

    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        for (auto &cell : view)
        {
            cell.weight = weight;
        }
    });
}

auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern, float weight)
    -> sequence::Cell
{
    if (weight <= 0.f)
    {
        throw std::runtime_error{"Weight must be greater than 0."};
    }

    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        for (auto &target_cell : view)
        {
            target_cell.weight = weight;
        }
    });
}

auto set_velocities(sequence::MusicElement element,
                    sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell = sequence::modify::set_velocity(
                    cell, pattern, evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

auto set_velocities(sequence::Cell cell, sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &target_cell = sequence.cells[i];
                target_cell = sequence::modify::set_velocity(
                    target_cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

auto set_delays(sequence::MusicElement element, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell = sequence::modify::set_delay(
                    cell, pattern, evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

auto set_delays(sequence::Cell cell, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &target_cell = sequence.cells[i];
                target_cell = sequence::modify::set_delay(
                    target_cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

auto set_gates(sequence::MusicElement element, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell = sequence::modify::set_gate(
                    cell, pattern, evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

auto set_gates(sequence::Cell cell, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::Cell
{
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &target_cell = sequence.cells[i];
                target_cell = sequence::modify::set_gate(
                    target_cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
            }
        }
    });
}

} // namespace xen::action
