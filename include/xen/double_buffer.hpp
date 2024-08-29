#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace xen
{

/**
 * Holds two data items of the same type, one to read from and one to write to.
 *
 * @details Useful to commiunicate data across threads. Once writes and one reads.
 */
template <typename T>
class DoubleBuffer
{
  public:
    DoubleBuffer() : current_read_buffer_{0}
    {
    }

  public:
    // Write data into the buffer
    auto write(T const &new_data) -> void
    {
        auto const write_buffer_index = 1 - current_read_buffer_.load();
        buffers_[write_buffer_index] = new_data;
        current_read_buffer_.store(write_buffer_index); // Swap Buffers
    }

    // Read data from the buffer
    [[nodiscard]] auto read() const -> T
    {
        auto const read_buffer_index = current_read_buffer_.load();
        return buffers_[read_buffer_index];
    }

  private:
    std::array<T, 2> buffers_;
    std::atomic<std::size_t> current_read_buffer_; // Index of the read buffer
};
} // namespace xen