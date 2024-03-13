#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

/**
 * Increment the state by applying a function to the selected Cell.
 *
 * This is a convinience function for Command implementations. It will create a copy of
 * the current state, call the given funtion with the selected cell as first parameter,
 * then save this state to the timeline.
 *
 * @param tl The timeline to operate on.
 * @param fn The function to apply to the selected Cell.
 * @param args The arguments to pass to the function.
 * @return State The new state.
 * @throw std::runtime_error If no Cell is selected.
 */
template <typename Fn, typename... Args>
auto increment_state(XenTimeline &tl, Fn &&fn, Args &&...args) -> void
{
    static_assert(
        std::is_invocable_r_v<sequence::Cell, Fn, sequence::Cell, Args...>,
        "Function must be invocable with a Cell and Args... and return a Cell.");

    auto const aux = tl.get_aux_state();
    auto state = tl.get_state().first;
    auto *selected = get_selected_cell(state.phrase, aux.selected);

    if (selected == nullptr)
    {
        throw std::runtime_error("No Cell Selected");
    }

    *selected = std::forward<Fn>(fn)(*selected, std::forward<Args>(args)...);

    tl.add_state(std::move(state));
}

} // namespace xen

namespace xen::action
{

[[nodiscard]] auto move_left(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_right(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_up(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto move_down(XenTimeline const &tl, std::size_t amount) -> AuxState;

[[nodiscard]] auto copy(XenTimeline const &tl) -> sequence::Cell;

[[nodiscard]] auto cut(XenTimeline const &tl) -> std::pair<sequence::Cell, State>;

[[nodiscard]] auto paste(XenTimeline const &tl,
                         std::optional<sequence::Cell> const &copy_buffer) -> State;

[[nodiscard]] auto duplicate(XenTimeline const &tl) -> std::pair<AuxState, State>;

[[nodiscard]] auto set_mode(XenTimeline const &tl, InputMode mode) -> AuxState;

[[nodiscard]] auto lift(XenTimeline const &tl) -> std::pair<State, AuxState>;

[[nodiscard]] auto shift_note_octave(XenTimeline const &tl,
                                     sequence::Pattern const &pattern, int amount)
    -> State;

[[nodiscard]] auto set_note_octave(XenTimeline const &tl,
                                   sequence::Pattern const &pattern, int octave)
    -> State;

[[nodiscard]] auto append_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>;

[[nodiscard]] auto insert_measure(XenTimeline const &tl, sequence::TimeSignature ts)
    -> std::pair<AuxState, State>;

[[nodiscard]] auto delete_cell(AuxState aux, State state) -> std::pair<AuxState, State>;

auto save_state(XenTimeline const &tl, std::filesystem::path const &filepath) -> void;

[[nodiscard]] auto load_state(std::filesystem::path const &filepath) -> State;

[[nodiscard]] auto set_timesignature(XenTimeline const &tl, sequence::TimeSignature ts)
    -> State;

[[nodiscard]] auto set_base_frequency(XenTimeline const &tl, float freq) -> State;

} // namespace xen::action