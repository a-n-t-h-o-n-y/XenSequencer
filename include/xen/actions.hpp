#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <juce_core/juce_core.h>

#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/measure.hpp>
#include <xen/modulator.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>

namespace xen
{

/**
 * Increment the state by applying a function to the selected Cell.
 *
 * @details This is a convinience function for Command implementations. It will create a
 * copy of the current state, call the given function with either the selected cell or
 * selected element as first parameter, then stage this state to the timeline. Does
 * not flag the Timeline for commit.
 *
 * @param state The state to mutate.
 * @param fn The function to apply to the selected Cell or MusicElement.
 * @param args The arguments to pass to the function.
 * @return TimelineState The updated state.
 * @throw std::runtime_error If no Cell is selected.
 */
template <typename Fn, typename... Args>
[[nodiscard]] auto increment_state(TimelineState state, Fn &&fn, Args &&...args)
    -> TimelineState
{
    constexpr bool supports_cell =
        std::is_invocable_r_v<sequence::Cell, Fn, sequence::Cell, Args...>;
    constexpr bool supports_element = std::is_invocable_r_v<
        sequence::MusicElement, Fn, sequence::MusicElement, Args...>;

    static_assert(
        supports_cell || supports_element,
        "Function must be invocable with a Cell or MusicElement and return the same type.");

    if (selection_kind(state.aux.selected) == SelectionKind::Element)
    {
        if constexpr (supports_element)
        {
            auto &selected =
                get_selected_element(state.sequencer.measure, state.aux.selected);
            selected =
                std::forward<Fn>(fn)(selected, std::forward<Args>(args)...);
        }
        else
        {
            throw std::runtime_error{"Action requires a whole-cell selection."};
        }
    }
    else
    {
        if constexpr (supports_cell)
        {
            auto &selected =
                get_selected_cell(state.sequencer.measure, state.aux.selected);
            selected = std::forward<Fn>(fn)(selected, std::forward<Args>(args)...);
        }
        else
        {
            throw std::runtime_error{"Action requires an element selection."};
        }
    }

    return state;
}

} // namespace xen

namespace xen::action
{

[[nodiscard]] auto move_left(EngineState const &state, ExecutionContext context,
                             std::size_t amount) -> ExecutionContext;

[[nodiscard]] auto move_right(EngineState const &state, ExecutionContext context,
                              std::size_t amount) -> ExecutionContext;

[[nodiscard]] auto move_up(EngineState const &state, ExecutionContext context,
                           std::size_t amount)
    -> ExecutionContext;

[[nodiscard]] auto move_down(EngineState const &state, ExecutionContext context,
                             std::size_t amount) -> ExecutionContext;

void copy(EngineState const &state, ExecutionContext const &context);

[[nodiscard]] auto paste(EngineState state, ExecutionContext const &context)
    -> EngineState;

[[nodiscard]] auto duplicate(TimelineState state) -> TimelineState;

[[nodiscard]] auto set_input_mode(ExecutionContext context, InputMode mode)
    -> ExecutionContext;

[[nodiscard]] auto lift(TimelineState state) -> TimelineState;

[[nodiscard]] auto shift_octave(EngineState state,
                                ExecutionContext const &context,
                                sequence::Pattern const &pattern, int amount)
    -> EngineState;

[[nodiscard]] auto set_note_octave(EngineState state,
                                   ExecutionContext const &context,
                                   sequence::Pattern const &pattern, int octave)
    -> EngineState;

[[nodiscard]] auto delete_cell(TimelineState state) -> TimelineState;

void save_measure(juce::File const &filepath, Measure const &measure);

[[nodiscard]] auto load_measure(juce::File const &filepath) -> Measure;

[[nodiscard]] auto set_base_frequency(EngineState state, float freq)
    -> EngineState;

[[nodiscard]] auto shift_scale_mode(Scale scale, int amount) -> Scale;

[[nodiscard]] auto shift_scale_index(std::optional<std::size_t> current,
                                     int shift_amount, std::size_t scale_count)
    -> std::optional<std::size_t>;

void flip_translate_direction(TranslateDirection &td);

[[nodiscard]] auto step(sequence::MusicElement element,
                        sequence::Pattern const &pattern, int pitch_distance,
                        float velocity_distance) -> sequence::MusicElement;

[[nodiscard]] auto step(sequence::Cell cell, sequence::Pattern const &pattern,
                        int pitch_distance, float velocity_distance) -> sequence::Cell;

[[nodiscard]] auto arp(sequence::MusicElement element,
                       sequence::Pattern const &pattern,
                       std::vector<int> const &intervals) -> sequence::MusicElement;

[[nodiscard]] auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
                       std::vector<int> const &intervals) -> sequence::Cell;

[[nodiscard]]
auto set_pitches(sequence::MusicElement element, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::MusicElement;

[[nodiscard]]
auto set_pitches(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_weight(sequence::Cell cell, float weight) -> sequence::Cell;

[[nodiscard]]
auto set_weights(sequence::MusicElement element, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::MusicElement;

[[nodiscard]]
auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_weights(sequence::MusicElement element, sequence::Pattern const &pattern,
                 float weight) -> sequence::MusicElement;

[[nodiscard]]
auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern, float weight)
    -> sequence::Cell;

[[nodiscard]]
auto set_velocities(sequence::MusicElement element,
                    sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::MusicElement;

[[nodiscard]]
auto set_velocities(sequence::Cell cell, sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_delays(sequence::MusicElement element, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::MusicElement;

[[nodiscard]]
auto set_delays(sequence::Cell cell, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_gates(sequence::MusicElement element, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::MusicElement;

[[nodiscard]]
auto set_gates(sequence::Cell cell, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::Cell;

} // namespace xen::action
