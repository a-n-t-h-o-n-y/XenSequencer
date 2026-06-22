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

#include <sequence/modify.hpp>
#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/copy_paste.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

#include "actions_internal.hpp"
#include "numeric.hpp"

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
    cell.elements.erase(
        std::next(std::begin(cell.elements),
                  (std::vector<sequence::MusicElement>::difference_type)element_index));
}

[[nodiscard]] auto checked_shift_pitch(sequence::MusicElement element,
                                       sequence::Pattern const &pattern, int amount)
    -> sequence::MusicElement;

void validate_pitch_shift(sequence::MusicElement const &element,
                          sequence::Pattern const &pattern, int amount);

void validate_octave(sequence::MusicElement const &element,
                     sequence::Pattern const &pattern, int octave,
                     std::size_t tuning_length);

void validate_pitch_shift(sequence::Cell const &cell, sequence::Pattern const &pattern,
                          int amount)
{
    for (auto const &element : cell.elements)
    {
        validate_pitch_shift(element, pattern, amount);
    }
}

void validate_pitch_shift(sequence::MusicElement const &element,
                          sequence::Pattern const &pattern, int amount)
{
    std::visit(
        [&, amount](auto const &value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, sequence::Note>)
            {
                (void)numeric::checked_add(value.pitch, amount,
                                           "Pitch shift exceeds int.");
            }
            else
            {
                auto cells = value.cells;
                auto view = sequence::PatternView{cells, pattern};
                for (auto &cell : view)
                {
                    validate_pitch_shift(cell, pattern, amount);
                }
            }
        },
        element);
}

void validate_octave(sequence::Cell const &cell, sequence::Pattern const &pattern,
                     int octave, std::size_t tuning_length)
{
    for (auto const &element : cell.elements)
    {
        validate_octave(element, pattern, octave, tuning_length);
    }
}

void validate_octave(sequence::MusicElement const &element,
                     sequence::Pattern const &pattern, int octave,
                     std::size_t tuning_length)
{
    auto const length =
        numeric::checked_cast<int>(tuning_length, "Tuning length exceeds int.");
    auto const octave_offset =
        numeric::checked_mul(octave, length, "Octave pitch exceeds int.");
    std::visit(
        [&](auto const &value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, sequence::Note>)
            {
                auto const degree = static_cast<int>(
                    utility::normalize_pitch(value.pitch, tuning_length));
                (void)numeric::checked_add(degree, octave_offset,
                                           "Octave pitch exceeds int.");
            }
            else
            {
                auto cells = value.cells;
                auto view = sequence::PatternView{cells, pattern};
                for (auto &cell : view)
                {
                    validate_octave(cell, pattern, octave, tuning_length);
                }
            }
        },
        element);
}

[[nodiscard]] auto checked_shift_pitch(sequence::Cell cell,
                                       sequence::Pattern const &pattern, int amount)
    -> sequence::Cell
{
    validate_pitch_shift(cell, pattern, amount);
    return sequence::modify::shift_pitch(std::move(cell), pattern, amount);
}

[[nodiscard]] auto checked_shift_pitch(sequence::MusicElement element,
                                       sequence::Pattern const &pattern, int amount)
    -> sequence::MusicElement
{
    validate_pitch_shift(element, pattern, amount);
    return sequence::modify::shift_pitch(std::move(element), pattern, amount);
}

[[nodiscard]] auto checked_modulator_pitch(Modulator const &mod, float t) -> int
{
    return numeric::floor_to_int(evaluate(mod, t),
                                 "Modulator pitch must be finite and fit in int.");
}

[[nodiscard]] auto checked_modulator_weight(Modulator const &mod, float t) -> float
{
    auto const weight = evaluate(mod, t);
    if (!std::isfinite(weight) || weight <= 0.f)
    {
        throw std::invalid_argument{
            "Modulator weight must be finite and greater than 0."};
    }
    return weight;
}

} // namespace

auto copy(EngineState const &state, SelectionPath const &selection)
    -> CopyBufferContent
{
    if (selection_kind(selection) == SelectionKind::Element)
    {
        return get_selected_element_const(state.measure, selection);
    }
    return get_selected_cell_const(state.measure, selection);
}

auto paste(EngineState &state, SelectionPath const &selection,
           CopyBufferContent const &content) -> SelectionMutation
{
    auto suggested_selection = selection;

    if (std::holds_alternative<sequence::Cell>(content))
    {
        auto replacement = std::get<sequence::Cell>(content);
        if (selection_kind(selection) == SelectionKind::Element)
        {
            auto *parent_cell = get_parent_cell_of_selection(state.measure, selection);
            *parent_cell = std::move(replacement);
            suggested_selection = select_parent_cell(selection);
        }
        else
        {
            auto &selected = get_selected_cell(state.measure, selection);
            selected = std::move(replacement);
        }
    }
    else
    {
        auto element = std::get<sequence::MusicElement>(content);
        if (selection_kind(selection) == SelectionKind::Element)
        {
            auto &parent_cell = *get_parent_cell_of_selection(state.measure, selection);
            auto const index = get_selected_element_index(selection);
            parent_cell.elements.insert(
                std::next(
                    std::begin(parent_cell.elements),
                    (std::vector<sequence::MusicElement>::difference_type)(index + 1)),
                std::move(element));
        }
        else
        {
            auto &selected = get_selected_cell(state.measure, selection);
            selected.elements.push_back(std::move(element));
        }
    }

    return SelectionMutation{.selection = std::move(suggested_selection)};
}

auto duplicate(EngineState &state, SelectionPath const &selection) -> SelectionMutation
{
    if (selection_kind(selection) == SelectionKind::Element)
    {
        auto &cell = get_selected_cell(state.measure, selection);
        auto const index = get_selected_element_index(selection);
        auto copy = cell.elements.at(index);
        cell.elements.insert(
            std::next(
                std::begin(cell.elements),
                (std::vector<sequence::MusicElement>::difference_type)(index + 1)),
            copy);
        return SelectionMutation{
            .selection =
                select_element_in_cell(select_parent_cell(selection), index + 1),
        };
    }

    auto selected_copy = get_selected_cell(state.measure, selection);

    auto new_selection = ::xen::move_right(state.measure, selection, 1);
    auto &selected = get_selected_cell(state.measure, new_selection);
    selected = selected_copy;
    return SelectionMutation{.selection = std::move(new_selection)};
}

auto lift(EngineState &state, SelectionPath const &selection) -> SelectionMutation
{
    if (selection_kind(selection) == SelectionKind::Element)
    {
        auto &cell = get_selected_cell(state.measure, selection);
        auto element = std::move(cell.elements.at(get_selected_element_index(selection)));
        cell.elements.clear();
        cell.elements.push_back(std::move(element));
        return SelectionMutation{.selection = select_parent_cell(selection)};
    }

    sequence::Cell *parent = get_parent_of_selected(state.measure, selection);
    if (parent == nullptr)
    {
        throw std::runtime_error{"Can't lift top level Cell."};
    }

    auto &cell = get_selected_cell(state.measure, selection);

    auto cell_copy = std::move(cell);
    *parent = std::move(cell_copy);

    return SelectionMutation{.selection = select_parent_cell(selection)};
}

auto shift_octave(EngineState state, SelectionPath const &selection,
                  sequence::Pattern const &pattern, int amount) -> EngineState
{
    auto const tuning_length = numeric::checked_cast<int>(state.tuning.intervals.size(),
                                                          "Tuning length exceeds int.");
    auto const shift =
        numeric::checked_mul(amount, tuning_length, "Octave shift exceeds int.");
    if (selection_kind(selection) == SelectionKind::Element)
    {
        auto &element = get_selected_element(state.measure, selection);
        element = checked_shift_pitch(std::move(element), pattern, shift);
    }
    else
    {
        auto &cell = get_selected_cell(state.measure, selection);
        cell = checked_shift_pitch(std::move(cell), pattern, shift);
    }
    return state;
}

auto set_note_octave(EngineState state, SelectionPath const &selection,
                     sequence::Pattern const &pattern, int octave) -> EngineState
{
    auto const tuning_length = state.tuning.intervals.size();
    if (selection_kind(selection) == SelectionKind::Element)
    {
        auto &element = get_selected_element(state.measure, selection);
        validate_octave(element, pattern, octave, tuning_length);
        element = sequence::modify::set_octave(element, pattern, octave, tuning_length);
    }
    else
    {
        auto &cell = get_selected_cell(state.measure, selection);
        validate_octave(cell, pattern, octave, tuning_length);
        cell = sequence::modify::set_octave(cell, pattern, octave, tuning_length);
    }
    return state;
}

auto delete_cell(EngineState &state, SelectionPath const &selection)
    -> SelectionMutation
{
    if (selection_kind(selection) == SelectionKind::Element)
    {
        auto &selected_cell = *get_parent_cell_of_selection(state.measure, selection);
        auto const index = get_selected_element_index(selection);
        erase_selected_element(selected_cell, index);

        if (selected_cell.elements.empty())
        {
            return SelectionMutation{.selection = select_parent_cell(selection)};
        }

        return SelectionMutation{
            .selection = select_element_in_cell(
                select_parent_cell(selection),
                std::min(index, selected_cell.elements.size() - 1)),
        };
    }

    auto &selected_cell = get_selected_cell(state.measure, selection);
    selected_cell.elements.clear();
    return SelectionMutation{.selection = selection};
}

auto set_base_frequency(EngineState state, float freq) -> EngineState
{
    state.base_frequency = std::clamp(freq, 20.f, 20'000.f);
    return state;
}

auto shift_pitch(sequence::MusicElement element, sequence::Pattern const &pattern,
                 int amount) -> sequence::MusicElement
{
    return checked_shift_pitch(std::move(element), pattern, amount);
}

auto shift_pitch(sequence::Cell cell, sequence::Pattern const &pattern, int amount)
    -> sequence::Cell
{
    return checked_shift_pitch(std::move(cell), pattern, amount);
}

auto shift_scale_mode(Scale scale, int amount) -> Scale
{
    validate_scale(scale);
    auto const size = static_cast<int>(scale.intervals.size());
    auto const amount_mod = amount % size;
    auto offset = static_cast<int>(scale.mode) - 1 + amount_mod;
    if (offset < 0)
    {
        offset += size;
    }
    else if (offset >= size)
    {
        offset -= size;
    }
    scale.mode = static_cast<std::uint8_t>(offset + 1);
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

    if (scale_count == std::numeric_limits<std::size_t>::max())
    {
        throw std::overflow_error{"Scale count is too large to rotate."};
    }
    if (current && *current >= scale_count)
    {
        throw std::invalid_argument{"Current scale index is out of range."};
    }

    auto const cycle_size = scale_count + 1;
    auto position = current.value_or(scale_count);
    auto const magnitude =
        shift_amount >= 0
            ? static_cast<std::uint64_t>(shift_amount)
            : static_cast<std::uint64_t>(-static_cast<std::int64_t>(shift_amount));
    auto const distance =
        static_cast<std::size_t>(magnitude % static_cast<std::uint64_t>(cycle_size));

    if (shift_amount >= 0)
    {
        position = (position + distance) % cycle_size;
    }
    else
    {
        position = distance <= position ? position - distance
                                        : cycle_size - (distance - position);
    }

    return position == scale_count ? std::nullopt
                                   : std::optional<std::size_t>{position};
}

void flip_translate_direction(TranslateDirection &td)
{
    td = (td == TranslateDirection::Up) ? TranslateDirection::Down
                                        : TranslateDirection::Up;
}

auto step(sequence::MusicElement element, sequence::Pattern const &pattern,
          int pitch_distance, float velocity_distance) -> sequence::MusicElement
{
    if (!std::isfinite(velocity_distance) || velocity_distance > 1.f ||
        velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &cell : view)
        {
            auto const index = numeric::checked_cast<int>(i, "Step index exceeds int.");
            cell = checked_shift_pitch(
                std::move(cell), {0, {1}},
                numeric::checked_mul(index, pitch_distance,
                                     "Step pitch shift exceeds int."));
            cell = sequence::modify::shift_velocity(cell, {0, {1}},
                                                    (float)i * velocity_distance);
            ++i;
        }
    });
}

auto step(sequence::Cell cell, sequence::Pattern const &pattern, int pitch_distance,
          float velocity_distance) -> sequence::Cell
{
    if (!std::isfinite(velocity_distance) || velocity_distance > 1.f ||
        velocity_distance < -1.f)
    {
        throw std::runtime_error{"velocity distance must be in the range: [-1, 1]"};
    }

    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &target_cell : view)
        {
            auto const index = numeric::checked_cast<int>(i, "Step index exceeds int.");
            target_cell = checked_shift_pitch(
                std::move(target_cell), {0, {1}},
                numeric::checked_mul(index, pitch_distance,
                                     "Step pitch shift exceeds int."));
            target_cell = sequence::modify::shift_velocity(
                target_cell, {0, {1}}, (float)i * velocity_distance);
            ++i;
        }
    });
}

auto arp(sequence::MusicElement element, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::MusicElement
{
    if (intervals.empty())
    {
        throw std::invalid_argument{"Arpeggio intervals must not be empty."};
    }
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &cell : view)
        {
            cell = checked_shift_pitch(std::move(cell), {0, {1}},
                                       intervals[i % intervals.size()]);
            ++i;
        }
    });
}

auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
         std::vector<int> const &intervals) -> sequence::Cell
{
    if (intervals.empty())
    {
        throw std::invalid_argument{"Arpeggio intervals must not be empty."};
    }
    return visit_sequence(std::move(cell), [&](sequence::Sequence &sequence) {
        auto view = sequence::PatternView{sequence.cells, pattern};
        auto i = std::size_t{0};
        for (auto &target_cell : view)
        {
            target_cell = checked_shift_pitch(std::move(target_cell), {0, {1}},
                                              intervals[i % intervals.size()]);
            ++i;
        }
    });
}

auto chord(sequence::Cell cell, std::vector<int> const &intervals,
           std::size_t tuning_size) -> sequence::Cell
{
    if (intervals.empty())
    {
        throw std::runtime_error{"Chord intervals must not be empty."};
    }

    for (auto i = std::size_t{0}; i < cell.elements.size(); ++i)
    {
        auto const base_interval = intervals[i % intervals.size()];
        auto const octave = numeric::checked_cast<int>(i / intervals.size(),
                                                       "Chord octave exceeds int.");
        auto const tuning_length =
            numeric::checked_cast<int>(tuning_size, "Tuning size exceeds int.");
        auto const octave_lift = numeric::checked_mul(octave, tuning_length,
                                                      "Chord octave lift exceeds int.");
        auto const shift = numeric::checked_add(base_interval, octave_lift,
                                                "Chord pitch shift exceeds int.");
        cell.elements[i] =
            checked_shift_pitch(std::move(cell.elements[i]), {0, {1}}, shift);
    }

    return cell;
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
                    checked_modulator_pitch(
                        mod, static_cast<float>(i) /
                                 static_cast<float>(sequence.cells.size())));
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
                    checked_modulator_pitch(
                        mod, static_cast<float>(i) /
                                 static_cast<float>(sequence.cells.size())));
            }
        }
    });
}

auto set_weight(sequence::Cell cell, float weight) -> sequence::Cell
{
    if (!std::isfinite(weight) || weight <= 0.f)
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
                cell.weight = checked_modulator_weight(
                    mod,
                    static_cast<float>(i) / static_cast<float>(sequence.cells.size()));
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
                target_cell.weight = checked_modulator_weight(
                    mod,
                    static_cast<float>(i) / static_cast<float>(sequence.cells.size()));
            }
        }
    });
}

auto set_weights(sequence::MusicElement element, sequence::Pattern const &pattern,
                 float weight) -> sequence::MusicElement
{
    if (!std::isfinite(weight) || weight <= 0.f)
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
    if (!std::isfinite(weight) || weight <= 0.f)
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

auto set_velocities(sequence::MusicElement element, sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::MusicElement
{
    return visit_sequence(std::move(element), [&](sequence::Sequence &sequence) {
        for (auto i = std::size_t{0}; i < sequence.cells.size(); ++i)
        {
            if (sequence::pattern_contains(pattern, i))
            {
                auto &cell = sequence.cells[i];
                cell = sequence::modify::set_velocity(
                    cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
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
                    cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
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
                    cell, pattern,
                    evaluate(mod, (float)i / (float)sequence.cells.size()));
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
