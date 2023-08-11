#pragma once

#include <cassert>
#include <chrono>
#include <mutex>
#include <vector>

#include <signals_light/signal.hpp>

namespace xen
{

/**
 * @brief A timeline of states.
 *
 * This class represents a timeline of states that can be navigated using undo and redo
 * operations. The timeline is thread-safe and can be used from multiple threads.
 *
 * @tparam State The type of the states stored in the timeline.
 */
template <typename State>
class Timeline
{
  public:
    /**
     * @brief A signal emitted when the state of the timeline changes.
     *
     * This signal is emitted whenever a new state is added to the timeline or the
     * current state is changed. The signal carries the new state as an argument.
     */
    sl::Signal<void(State)> state_changed;

  public:
    explicit Timeline(State state)
        : history_{std::move(state)}, current_state_index_{0},
          last_update_time_{std::chrono::high_resolution_clock::now()}
    {
    }

  public:
    /**
     * @brief Add a new state to the timeline.
     * @param state The new state to be added.
     * @return void.
     */
    auto add_state(State state) -> void
    {
        std::lock_guard<std::mutex> lock{mutex_};

        if (current_state_index_ + 1 < history_.size())
        {
            history_.resize(current_state_index_ + 1);
        }
        history_.push_back(std::move(state));
        ++current_state_index_;
        state_changed.emit(history_[current_state_index_]);
        last_update_time_ = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Get the current state.
     * @return The current state.
     */
    [[nodiscard]] auto get_state() const -> State
    {
        std::lock_guard<std::mutex> lock{mutex_};

        assert(!history_.empty());
        return history_[current_state_index_];
    }

    /**
     * @brief Undo the last operation.
     * @return true if undo was successful, false otherwise.
     */
    auto undo() -> bool
    {
        std::lock_guard<std::mutex> lock{mutex_};

        if (current_state_index_ == 0)
        {
            return false;
        }
        state_changed.emit(history_[--current_state_index_]);
        last_update_time_ = std::chrono::high_resolution_clock::now();
        return true;
    }

    /**
     * @brief Redo the last undone operation.
     * @return true if redo was successful, false otherwise.
     */
    auto redo() -> bool
    {
        std::lock_guard<std::mutex> lock{mutex_};

        if (current_state_index_ + 1 < history_.size())
        {
            state_changed.emit(history_[++current_state_index_]);
            last_update_time_ = std::chrono::high_resolution_clock::now();
            return true;
        }
        return false;
    }

    /**
     * @brief Get the time of the last update.
     * @return The time of the last update.
     * @note This function is thread-safe.
     */
    [[nodiscard]] auto get_last_update_time() const
        -> std::chrono::high_resolution_clock::time_point
    {
        std::lock_guard<std::mutex> lock{mutex_};
        return last_update_time_;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<State> history_;
    size_t current_state_index_;
    std::chrono::high_resolution_clock::time_point last_update_time_;
};

} // namespace xen