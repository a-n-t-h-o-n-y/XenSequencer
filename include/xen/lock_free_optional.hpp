#pragma once

#include <atomic>
#include <concepts>
#include <optional>
#include <type_traits>

namespace xen
{

/**
 * A lock-free optional value that is consumed on read.
 * @tparam T The type of the optional value.
 */
template <typename T>
class LockFreeOptional
{
  public:
    LockFreeOptional() : has_value_{false}
    {
    }

  public:
    /**
     * Set the value of the optional.
     * @param new_value The new value to set.
     * @note This operation is only safe for a single producer thread.
     */
    void set(T const &new_value) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        value_ = new_value; // Assume T is trivially copyable
        has_value_.store(true, std::memory_order_release);
    }

    /**
     * Set the value of the optional via move.
     * @param new_value The new value to set.
     * @note This operation is only safe for a single producer thread.
     */
    void set(T &&new_value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        value_ = std::move(new_value);
        has_value_.store(true, std::memory_order_release);
    }

    /**
     * Get the value of the optional and consume it.
     * @return The value of the optional, or std::nullopt if no value is present.
     * @note This operation is only safe for a single consumer thread.
     */
    [[nodiscard]] auto get() -> std::optional<T>
    {
        if (has_value_.exchange(false, std::memory_order_acquire))
        {
            return value_;
        }
        else
        {
            return std::nullopt;
        }
    }

  private:
    std::atomic<bool> has_value_;
    T value_;
};

} // namespace xen