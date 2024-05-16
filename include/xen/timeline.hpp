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
    explicit Timeline(State state) : stage_{std::move(state)}, timeline_{stage_}
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
     * @details This always appends the staged state to the timeline, which is a copy of
     * the previous state if nothing has been staged since the previous commit. Use
     * set_commit_flag() and get_commit_flag() to notify yourself if a commit should
     * happen. If the timeline is in the past, the future is truncated.
     */
    auto commit() -> void
    {
        at_ = at_ + 1;
        timeline_.resize(at_);
        timeline_.push_back(stage_);
        should_commit_ = false;
    }

    /**
     * Retrieve the latest state.
     *
     * @details This is the state that was last staged or a previous commit if undo has
     * been called.
     */
    [[nodiscard]] auto get_state() const -> State
    {
        return stage_;
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
            stage_ = timeline_[at_];
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
            stage_ = timeline_[at_];
            return true;
        }
        return false;
    }

    /**
     * Set an internal flag that can be used by the user to determine if a commit should
     * happen.
     *
     * @details This is not used by the Timeline class itself, but can be used by the
     * user to determine if a commit should happen. It is set to `false` by Timeline
     * after a commit.
     */
    auto set_commit_flag() -> void
    {
        should_commit_ = true;
    }

    /**
     * Get the commit flag value.
     */
    [[nodiscard]] auto get_commit_flag() const -> bool
    {
        return should_commit_;
    }

    /**
     * Reset the staged state to the current commit point.
     *
     * @details This erases any staged changes that have not been committed. Useful if
     * you need to revert state that has not been committed because of an error.
     */
    auto reset_stage() -> void
    {
        stage_ = timeline_[at_];
    }

  private:
    State stage_; // Staged state to be committed. Also the 'current' state.
    std::vector<State> timeline_;
    std::size_t at_{0};
    bool should_commit_{false};
};

} // namespace xen