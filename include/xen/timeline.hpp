#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace xen
{

/**
 * A timeline/history of States.
 *
 * @details The timeline can have State staged to it, which can be written to the
 * timeline with a commit() call. You can move through commit history with undo/redo
 * commands and truncate history with new writes after an undo.
 * @tparam State The type of the states stored in the timeline.
 */
template <typename State>
class Timeline
{
  public:
    /**
     * Construct a new timeline with an initial state.
     *
     * @details The Timeline is never empty, there is always an initial state.
     */
    explicit Timeline(State state)
        : stage_{std::move(state)}, timeline_{{stage_, id_origin_++}}
    {
    }

  public:
    /**
     * Stage a new state that can be committed to the Timeline later.
     *
     * @details Any subsequent calls will overwrite the staged state. Store changes by
     * committing them once staged.
     * @param state The new state to be staged.
     */
    auto stage(State state) -> void
    {
        stage_ = std::move(state);
    }

    /**
     * Commit previously staged state to the timeline.
     *
     * @details Appends only when the staged state differs from the current commit. If
     * the timeline is in the past, a changed commit truncates the future. A no-op
     * preserves redo history.
     */
    auto commit() -> bool
    {
        if (stage_ == timeline_[at_].first)
        {
            return false;
        }
        at_ = at_ + 1;
        timeline_.resize(at_);
        timeline_.push_back({stage_, id_origin_++});
        return true;
    }

    /**
     * Retrieve the current state.
     *
     * @details This is the state that was last staged or a previous commit if undo has
     * been called.
     */
    [[nodiscard]] auto get_state() const -> State
    {
        return stage_;
    }

    /**
     * Retrieve the current committed state at the timeline cursor.
     */
    [[nodiscard]] auto get_committed_state() const -> State
    {
        return timeline_[at_].first;
    }

    /**
     * Return the unique commit ID for the most recent commit state. Does not change on
     * staged state.
     */
    [[nodiscard]] auto get_current_commit_id() const -> int
    {
        return timeline_[at_].second;
    }

    /**
     * Return the unique commit ID for the next commit.
     */
    [[nodiscard]] auto get_next_commit_id() const -> int
    {
        return id_origin_;
    }

    /**
     * Go back one state in the timeline.
     *
     * @details This causes get_state() to return the previous state. If the timeline is
     * at the beginning, nothing happens. The current staged state is overwritten by the
     * previous state and any changes will occur from there. If a commit is not made, a
     * redo() is possible to go back to the latest state.
     */
    auto undo() -> bool
    {
        if (at_ > 0)
        {
            at_ = at_ - 1;
            stage_ = timeline_[at_].first;
            return true;
        }
        return false;
    }

    /**
     * Go forward one state in the timeline.
     *
     * @details This causes get_state() to return the next state. If the timeline is at
     * the end, nothing happens. The current staged state is overwritten by the next
     * state and any changes will occur from there.
     */
    auto redo() -> bool
    {
        if (at_ + 1 < std::size(timeline_))
        {
            at_ = at_ + 1;
            stage_ = timeline_[at_].first;
            return true;
        }
        return false;
    }

    /**
     * Reset the staged state to the current commit point.
     *
     * @details This erases any staged changes that have not been committed. Useful if
     * you need to revert state that has not been committed because of an error.
     */
    auto reset_stage() -> void
    {
        stage_ = timeline_[at_].first;
    }

  private:
    int id_origin_{0};
    State stage_; // Staged state to be committed. Also the 'current' state.
    std::vector<std::pair<State, int>> timeline_; // [state, commit ID]
    std::size_t at_{0};
};

} // namespace xen
