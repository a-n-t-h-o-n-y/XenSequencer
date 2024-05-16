#pragma once

#include <array>
#include <cstddef>

#include <juce_core/juce_core.h>

/**
 * A lock-free FIFO queue based on JUCE's AbstractFifo.
 *
 * @tparam T The type of elements in the queue.
 * @tparam Capacity The maximum number of elements the queue can hold.
 */
template <typename T, std::size_t Capacity>
class LockFreeQueue
{
  public:
    /**
     * Constructs the Lock-Free FIFO Queue.
     */
    LockFreeQueue() : abstract_fifo_(Capacity)
    {
    }

  public:
    /**
     * Attempts to push an element into the queue.
     *
     * @param value The value to be pushed.
     * @return true if the value was successfully pushed, false otherwise.
     */
    [[nodiscard]] auto push(T const &value) -> bool
    {
        int start1, size1, start2, size2;
        abstract_fifo_.prepareToWrite(1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            buffer_[start1] = value;
            abstract_fifo_.finishedWrite(1);
            return true;
        }

        return false;
    }

    /**
     * Attempts to pop an element from the queue.
     *
     * @details Does not write to \p value if the queue is empty.
     * @param value Reference to the variable where the popped value will be stored.
     * @return true if a value was successfully popped, false otherwise.
     */
    [[nodiscard]] auto pop(T &value) -> bool
    {
        int start1, size1, start2, size2;
        abstract_fifo_.prepareToRead(1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            value = buffer_[start1];
            abstract_fifo_.finishedRead(1);
            return true;
        }

        return false;
    }

    /**
     * Checks if the queue is empty.
     *
     * @return true if the queue is empty, false otherwise.
     */
    [[nodiscard]] auto is_empty() const -> bool
    {
        return abstract_fifo_.getNumReady() == 0;
    }

    /**
     * Checks if the queue is full.
     *
     * @return true if the queue is full, false otherwise.
     */
    [[nodiscard]] auto is_full() const -> bool
    {
        return abstract_fifo_.getFreeSpace() == 0;
    }

    /**
     * Returns the number of elements in the queue.
     */
    [[nodiscard]] auto size() const -> std::size_t
    {
        return abstract_fifo_.getNumReady();
    }

  private:
    std::array<T, Capacity> buffer_;
    juce::AbstractFifo abstract_fifo_;
};
