#pragma once

#include <cstddef>
#include <optional>
#include <utility>

#include <sequence/sequence.hpp>

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

[[nodiscard]] auto copy(XenTimeline const &tl) -> sequence::Cell;

[[nodiscard]] auto cut(XenTimeline const &tl) -> std::pair<sequence::Cell, State>;

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

} // namespace xen::action