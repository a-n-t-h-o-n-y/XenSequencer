#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace xen
{

/**
 * Thread-safe double buffer for single producer, single consumer scenarios.
 * @tparam T Type of data to be buffered
 * @warning T must be trivially copyable or implement thread-safe copy operations
 */
template <typename T>
class DoubleBuffer
{
    static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");

  public:
    DoubleBuffer() : current_read_buffer_{0}
    {
    }

  public:
    /**
     * Write new data to the write buffer and swap buffers.
     * @param new_data Data to write
     * @note This operation is only safe for a single producer thread
     */
    void write(T const &new_data) noexcept
    {
        const auto write_buffer_index =
            1 - current_read_buffer_.load(std::memory_order_acquire);

        buffers_[write_buffer_index] = new_data;

        // Swap buffers
        current_read_buffer_.store(write_buffer_index, std::memory_order_release);
    }

    /**
     * Read current data from the read buffer.
     * @return Copy of current data
     * @note This operation is only safe for a single consumer thread
     */
    [[nodiscard]] auto read() const noexcept -> T
    {
        const auto read_buffer_index =
            current_read_buffer_.load(std::memory_order_acquire);

        return buffers_[read_buffer_index];
    }

  private:
    // alignas to prevent false sharing
    alignas(64) std::array<T, 2> buffers_;

    // Index of the read buffer
    alignas(64) std::atomic<std::size_t> current_read_buffer_;
};

} // namespace xen