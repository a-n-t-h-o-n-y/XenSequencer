#pragma once

#include <cstddef>
#include <optional>
#include <utility>

#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include "input_mode.hpp"
#include "selection.hpp"
#include "state.hpp"
#include "xen_timeline.hpp"

namespace xen::action
{

[[nodiscard]] auto move_left(XenTimeline const &tl) -> AuxState;

[[nodiscard]] auto move_right(XenTimeline const &tl) -> AuxState;

[[nodiscard]] auto move_up(XenTimeline const &tl) -> AuxState;

[[nodiscard]] auto move_down(XenTimeline const &tl) -> AuxState;

[[nodiscard]] auto copy(XenTimeline const &tl) -> std::optional<sequence::Cell>;

[[nodiscard]] auto cut(XenTimeline const &tl)
    -> std::optional<std::pair<sequence::Cell, State>>;

[[nodiscard]] auto paste(XenTimeline const &tl,
                         std::optional<sequence::Cell> const &copy_buffer)
    -> std::optional<State>;

[[nodiscard]] auto duplicate(XenTimeline const &tl) -> std::pair<AuxState, State>;

[[nodiscard]] auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState;

[[nodiscard]] auto note(XenTimeline const &tl, int interval, float velocity,
                        float delay, float gate) -> State;

[[nodiscard]] auto rest(XenTimeline const &tl) -> State;

[[nodiscard]] auto flip(XenTimeline const &tl) -> State;

[[nodiscard]] auto split(XenTimeline const &tl, std::size_t count) -> State;

[[nodiscard]] auto extract(XenTimeline const &tl) -> std::pair<State, AuxState>;

[[nodiscard]] auto shift_note(XenTimeline const &tl, int amount) -> State;

[[nodiscard]] auto shift_note_octave(XenTimeline const &tl, int amount) -> State;

[[nodiscard]] auto shift_velocity(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto shift_delay(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto shift_gate(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto set_note(XenTimeline const &tl, int interval) -> State;

[[nodiscard]] auto set_note_octave(XenTimeline const &tl, int octave) -> State;

[[nodiscard]] auto set_velocity(XenTimeline const &tl, float velocity) -> State;

[[nodiscard]] auto set_delay(XenTimeline const &tl, float delay) -> State;

[[nodiscard]] auto set_gate(XenTimeline const &tl, float gate) -> State;

[[nodiscard]] auto add_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>;

[[nodiscard]] auto delete_cell(AuxState aux, State state) -> std::pair<AuxState, State>;

auto save_state(XenTimeline const &tl, std::string const &filepath) -> void;

[[nodiscard]] auto load_state(std::string const &filepath) -> State;

[[nodiscard]] auto rotate(XenTimeline const &tl, int amount) -> State;

[[nodiscard]] auto reverse(XenTimeline const &tl) -> State;

[[nodiscard]] auto mirror(XenTimeline const &tl, int center_note) -> State;

[[nodiscard]] auto shuffle(XenTimeline const &tl) -> State;

[[nodiscard]] auto compress(XenTimeline const &tl, std::size_t amount) -> State;

[[nodiscard]] auto stretch(XenTimeline const &tl, std::size_t amount) -> State;

[[nodiscard]] auto quantize(XenTimeline const &tl) -> State;

[[nodiscard]] auto swing(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto randomize_notes(XenTimeline const &tl, int min, int max) -> State;

[[nodiscard]] auto randomize_velocities(XenTimeline const &tl, float min, float max)
    -> State;

[[nodiscard]] auto randomize_delays(XenTimeline const &tl, float min, float max)
    -> State;

[[nodiscard]] auto randomize_gates(XenTimeline const &tl, float min, float max)
    -> State;

[[nodiscard]] auto humanize_velocities(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto humanize_delays(XenTimeline const &tl, float amount) -> State;

[[nodiscard]] auto humanize_gates(XenTimeline const &tl, float amount) -> State;

} // namespace xen::action