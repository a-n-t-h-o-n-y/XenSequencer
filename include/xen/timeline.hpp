#pragma once

#include <atomic>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace xen
{

class HistoryEntryId
{
  public:
    explicit constexpr HistoryEntryId(std::uint64_t value = 0) noexcept : value_{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
    {
        return value_;
    }

    auto operator<=>(HistoryEntryId const &) const = default;

  private:
    std::uint64_t value_;
};

class ProjectRevision
{
  public:
    explicit constexpr ProjectRevision(std::uint64_t value = 0) noexcept : value_{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
    {
        return value_;
    }

    auto operator<=>(ProjectRevision const &) const = default;

  private:
    std::uint64_t value_;
};

namespace detail
{

inline auto allocate_history_entry_id() noexcept -> HistoryEntryId
{
    static auto next_value = std::atomic<std::uint64_t>{1};
    return HistoryEntryId{next_value.fetch_add(1, std::memory_order_relaxed)};
}

inline auto allocate_project_revision() noexcept -> ProjectRevision
{
    static auto next_value = std::atomic<std::uint64_t>{1};
    return ProjectRevision{next_value.fetch_add(1, std::memory_order_relaxed)};
}

} // namespace detail

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
        : stage_{std::move(state)},
          timeline_{{stage_, detail::allocate_history_entry_id()}},
          revision_{detail::allocate_project_revision()}
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
        if (stage_ == timeline_[at_].state)
        {
            return false;
        }
        at_ = at_ + 1;
        timeline_.resize(at_);
        timeline_.push_back({stage_, detail::allocate_history_entry_id()});
        revision_ = detail::allocate_project_revision();
        return true;
    }

    /**
     * Amend the current history tip when its identity matches the expected entry.
     */
    auto amend_current(HistoryEntryId expected_entry_id, State state) -> bool
    {
        if (at_ + 1 != std::size(timeline_) || timeline_[at_].id != expected_entry_id ||
            state == timeline_[at_].state)
        {
            return false;
        }

        timeline_[at_].state = std::move(state);
        stage_ = timeline_[at_].state;
        revision_ = detail::allocate_project_revision();
        return true;
    }

    /**
     * Replace all history with a new root, including when the project data is equal.
     */
    auto replace_history(State state) -> void
    {
        stage_ = std::move(state);
        timeline_ = {{stage_, detail::allocate_history_entry_id()}};
        at_ = 0;
        revision_ = detail::allocate_project_revision();
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
        return timeline_[at_].state;
    }

    /**
     * Return the immutable identity of the current history entry.
     */
    [[nodiscard]] auto get_current_entry_id() const noexcept -> HistoryEntryId
    {
        return timeline_[at_].id;
    }

    /**
     * Return the current authoritative project generation.
     */
    [[nodiscard]] auto get_project_revision() const noexcept -> ProjectRevision
    {
        return revision_;
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
            stage_ = timeline_[at_].state;
            revision_ = detail::allocate_project_revision();
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
            stage_ = timeline_[at_].state;
            revision_ = detail::allocate_project_revision();
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
        stage_ = timeline_[at_].state;
    }

  private:
    struct Entry
    {
        State state;
        HistoryEntryId id;
    };

    State stage_; // Staged state to be committed. Also the 'current' state.
    std::vector<Entry> timeline_;
    std::size_t at_{0};
    ProjectRevision revision_;
};

} // namespace xen
