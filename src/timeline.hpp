#pragma once

#include <cassert>
#include <chrono>
#include <mutex>
#include <utility>
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
template <typename State, typename AuxiliaryState>
class Timeline
{
  public:
    /**
     * @brief A signal emitted when the state of the timeline changes.
     *
     * This signal is emitted whenever a new state is added to the timeline or the
     * current state is changed. The signal carries the new state as an argument.
     */
    sl::Signal<void(State const &, AuxiliaryState const &)> on_state_change;

    /**
     * @brief A signal emitted when the auxiliary state of the timeline changes.
     */
    sl::Signal<void(State const &, AuxiliaryState const &)> on_aux_change;

  public:
    explicit Timeline(State state, AuxiliaryState aux)
        : history_{{std::move(state), std::move(aux)}}, current_state_index_{0},
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
        history_.push_back({std::move(state), auxiliary_state_});
        ++current_state_index_;
        this->emit_state_change();
    }

    /**
     * @brief Get the current state.
     * @return The current state, the auxiliary state is from the last call to add_state
     * or undo/redo, not necessarily the latest.
     */
    [[nodiscard]] auto get_state() const -> std::pair<State, AuxiliaryState>
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
        --current_state_index_;

        // Update aux state to reflect undo
        auxiliary_state_ = history_[current_state_index_].second;
        this->emit_state_change();
        this->emit_aux_change();
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
            ++current_state_index_;

            // Update aux state to reflect redo
            auxiliary_state_ = history_[current_state_index_].second;
            this->emit_state_change();
            this->emit_aux_change();
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

    /**
     * @brief Set the auxiliary state.
     * @param aux The auxiliary state.
     * @note This function is thread-safe and does not update the last update time or
     * emit the on_state_change signal.
     */
    auto set_aux_state(AuxiliaryState aux) -> void
    {
        std::lock_guard<std::mutex> lock{mutex_};
        auxiliary_state_ = std::move(aux);
        this->emit_aux_change();
    }

    /**
     * @brief Get the auxiliary state.
     * @return A copy of the current auxiliary state.
     * @note This function is thread-safe.
     */
    [[nodiscard]] auto get_aux_state() const -> AuxiliaryState
    {
        std::lock_guard<std::mutex> lock{mutex_};
        return auxiliary_state_;
    }

  private:
    auto emit_state_change() -> void
    {
        on_state_change.emit(history_[current_state_index_].first,
                             history_[current_state_index_].second);
        last_update_time_ = std::chrono::high_resolution_clock::now();
    }

    auto emit_aux_change() -> void
    {
        on_aux_change.emit(history_[current_state_index_].first, auxiliary_state_);
    }

  private:
    mutable std::mutex mutex_;
    std::vector<std::pair<State, AuxiliaryState>> history_;
    size_t current_state_index_;
    AuxiliaryState auxiliary_state_;
    std::chrono::high_resolution_clock::time_point last_update_time_;
};

} // namespace xen