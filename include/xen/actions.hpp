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
 * @param tl The timeline to operate on.
 * @param fn The function to apply to the selected Cell.
 * @param args The arguments to pass to the function.
 * @return SequencerState The new state.
 * @throw std::runtime_error If no Cell is selected.
 */
template <typename Fn, typename... Args>
void increment_state(XenTimeline &tl, Fn &&fn, Args &&...args)
{
    static_assert(
        std::is_invocable_r_v<sequence::Cell, Fn, sequence::Cell, Args...>,
        "Function must be invocable with a Cell and Args... and return a Cell.");

    auto [state, aux] = tl.get_state();
    auto &selected = get_selected_cell(state.sequence_bank, aux.selected);

    selected = std::forward<Fn>(fn)(selected, std::forward<Args>(args)...);

    tl.stage({std::move(state), std::move(aux)});
}

} // namespace xen

namespace xen::action
{

[[nodiscard]] auto move_left(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_right(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_up(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_down(XenTimeline const &tl, std::size_t amount) -> AuxState;

void copy(XenTimeline const &tl);

[[nodiscard]] auto cut(XenTimeline const &tl) -> SequencerState;

[[nodiscard]] auto paste(XenTimeline const &tl) -> SequencerState;

[[nodiscard]] auto duplicate(XenTimeline const &tl) -> TrackedState;

[[nodiscard]] auto set_input_mode(XenTimeline const &tl, InputMode mode) -> AuxState;

[[nodiscard]] auto lift(XenTimeline const &tl) -> TrackedState;

[[nodiscard]] auto shift_octave(XenTimeline const &tl, sequence::Pattern const &pattern,
                                int amount) -> SequencerState;

[[nodiscard]] auto set_note_octave(XenTimeline const &tl,
                                   sequence::Pattern const &pattern, int octave)
    -> SequencerState;

[[nodiscard]] auto delete_cell(TrackedState state) -> TrackedState;

void save_measure(juce::File const &filepath, sequence::Measure const &measure);

[[nodiscard]] auto load_measure(juce::File const &filepath) -> sequence::Measure;

void save_sequence_bank(SequenceBank const &bank,
                        std::array<std::string, 16> const &sequence_names,
                        juce::File const &filepath);

[[nodiscard]] auto load_sequence_bank(juce::File const &filepath)
    -> std::pair<SequenceBank, std::array<std::string, 16>>;

[[nodiscard]] auto set_base_frequency(XenTimeline const &tl, float freq)
    -> SequencerState;

[[nodiscard]] auto set_selected_sequence(AuxState aux, int index) -> AuxState;

[[nodiscard]] auto shift_scale_mode(Scale scale, int amount) -> Scale;

[[nodiscard]] auto shift_scale_index(std::optional<std::size_t> current,
                                     int shift_amount, std::size_t scale_count)
    -> std::optional<std::size_t>;

[[nodiscard]] auto step(sequence::Cell cell, sequence::Pattern const &pattern,
                        int pitch_distance, float velocity_distance) -> sequence::Cell;

[[nodiscard]] auto arp(sequence::Cell cell, sequence::Pattern const &pattern,
                       std::vector<int> const &intervals) -> sequence::Cell;

[[nodiscard]]
auto set_weight(sequence::Cell cell, float weight) -> sequence::Cell;

[[nodiscard]]
auto set_weights(sequence::Cell cell, Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_velocities(sequence::Cell cell, Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_delays(sequence::Cell cell, Modulator const &mod) -> sequence::Cell;

[[nodiscard]]
auto set_gates(sequence::Cell cell, Modulator const &mod) -> sequence::Cell;

} // namespace xen::action
