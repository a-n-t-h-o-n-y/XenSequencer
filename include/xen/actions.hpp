#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <juce_core/juce_core.h>

#include <sequence/measure.hpp>
#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/modulator.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>

namespace xen
{

/**
 * Increment the state by applying a function to the selected Cell.
 *
 * @details This is a convinience function for Command implementations. It will create a
 * copy of the current state, call the given funtion with the selected cell as first
 * parameter, then stage this state to the timeline. Does not flag the Timeline for
 * commit.
 *
 * @param state The state to mutate.
 * @param fn The function to apply to the selected Cell.
 * @param args The arguments to pass to the function.
 * @return TimelineState The updated state.
 * @throw std::runtime_error If no Cell is selected.
 */
template <typename Fn, typename... Args>
[[nodiscard]] auto increment_state(TimelineState state, Fn &&fn, Args &&...args)
    -> TimelineState
{
    static_assert(
        std::is_invocable_r_v<sequence::Cell, Fn, sequence::Cell, Args...>,
        "Function must be invocable with a Cell and Args... and return a Cell.");

    auto &selected =
        get_selected_cell(state.sequencer.sequence_bank, state.aux.selected);

    selected = std::forward<Fn>(fn)(selected, std::forward<Args>(args)...);
    return state;
}

} // namespace xen

namespace xen::action
{

[[nodiscard]] auto move_left(EngineState const &state, ExecutionContext context,
                             std::size_t amount) -> ExecutionContext;

[[nodiscard]] auto move_right(EngineState const &state, ExecutionContext context,
                              std::size_t amount) -> ExecutionContext;

[[nodiscard]] auto move_up(ExecutionContext context, std::size_t amount)
    -> ExecutionContext;

[[nodiscard]] auto move_down(EngineState const &state, ExecutionContext context,
                             std::size_t amount) -> ExecutionContext;

void copy(EngineState const &state, ExecutionContext const &context);

[[nodiscard]] auto cut(EngineState state, ExecutionContext const &context)
    -> EngineState;

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

void save_measure(juce::File const &filepath, sequence::Measure const &measure);

[[nodiscard]] auto load_measure(juce::File const &filepath) -> sequence::Measure;

void save_sequence_bank(SequenceBank const &bank,
                        std::array<std::string, 16> const &sequence_names,
                        juce::File const &filepath);

[[nodiscard]] auto load_sequence_bank(juce::File const &filepath)
    -> std::pair<SequenceBank, std::array<std::string, 16>>;

[[nodiscard]] auto set_base_frequency(EngineState state, float freq)
    -> EngineState;

[[nodiscard]] auto set_selected_sequence(ExecutionContext context, int index)
    -> ExecutionContext;

[[nodiscard]] auto shift_scale_mode(Scale scale, int amount) -> Scale;

[[nodiscard]] auto shift_scale_index(std::optional<std::size_t> current,
                                     int shift_amount, std::size_t scale_count)
    -> std::optional<std::size_t>;

void flip_translate_direction(TranslateDirection &td);

[[nodiscard]] auto step(sequence::Cell cell, sequence::Pattern const &pattern,
                        int pitch_distance, float velocity_distance) -> sequence::Cell;

[[nodiscard]] auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
                       std::vector<int> const &intervals) -> sequence::Cell;

[[nodiscard]]
auto set_pitches(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_weight(sequence::Cell cell, float weight) -> sequence::Cell;

[[nodiscard]]
auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern,
                 Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_weights(sequence::Cell cell, sequence::Pattern const &pattern, float weight)
    -> sequence::Cell;

[[nodiscard]]
auto set_velocities(sequence::Cell cell, sequence::Pattern const &pattern,
                    Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_delays(sequence::Cell cell, sequence::Pattern const &pattern,
                Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_gates(sequence::Cell cell, sequence::Pattern const &pattern,
               Modulator const &mod) -> sequence::Cell;

} // namespace xen::action
