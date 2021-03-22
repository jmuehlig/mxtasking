#pragma once
#include "load.h"
#include "prefetch_slot.h"
#include "task.h"
#include <array>
#include <cstdint>
#include <mx/system/cache.h>
#include <utility>

namespace mx::tasking {
/**
 * The task buffer holds tasks that are ready to execute.
 * The buffer is realized as a ring buffer with a fixed size.
 * All empty slots are null pointers.
 */
template <std::size_t S> class TaskBuffer
{
private:
    class Slot
    {
    public:
        constexpr Slot() noexcept = default;
        ~Slot() noexcept = default;

        void task(TaskInterface *task) noexcept { _task = task; }
        [[nodiscard]] TaskInterface *consume_task() noexcept { return std::exchange(_task, nullptr); }

        void prefetch() noexcept { _prefetch_slot(); }
        void prefetch(TaskInterface *task) noexcept { _prefetch_slot = task; }

        bool operator==(std::nullptr_t) const noexcept { return _task == nullptr; }
        bool operator!=(std::nullptr_t) const noexcept { return _task != nullptr; }

    private:
        TaskInterface *_task{nullptr};
        PrefetchSlot _prefetch_slot{};
    };

public:
    constexpr explicit TaskBuffer(const std::uint8_t prefetch_distance) noexcept : _prefetch_distance(prefetch_distance)
    {
    }
    ~TaskBuffer() noexcept = default;

    /**
     * @return True, when the buffer is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _buffer[_head] == nullptr; }

    /**
     * @return Number of tasks in the buffer.
     */
    [[nodiscard]] std::uint16_t size() const noexcept
    {
        return _tail >= _head ? (_tail - _head) : (S - (_head - _tail));
    }

    /**
     * @return Number of maximal tasks of the buffer.
     */
    constexpr auto max_size() const noexcept { return S; }

    /**
     * @return Number of free slots.
     */
    [[nodiscard]] std::uint16_t available_slots() const noexcept { return S - size(); }

    /**
     * @return The next task in the buffer; the slot will be available after.
     */
    TaskInterface *next() noexcept;

    /**
     * Takes out tasks from the given queue and inserts them into the buffer.
     * @param from_queue Queue to take tasks from.
     * @param count Number of maximal tasks to take out of the queue.
     * @return Number of retrieved tasks.
     */
    template <class Q> std::uint16_t fill(Q &from_queue, std::uint16_t count) noexcept;

private:
    // Prefetch distance.
    const std::uint8_t _prefetch_distance;

    // Index of the first element in the buffer.
    std::uint16_t _head{0U};

    // Index of the last element in the buffer.
    std::uint16_t _tail{0U};

    // Array with task-slots.
    std::array<Slot, S> _buffer{};

    /**
     * Normalizes the index with respect to the size.
     * @param index Index.
     * @return Normalized index.
     */
    static std::uint16_t normalize(const std::uint16_t index) noexcept { return index & (S - 1U); }

    /**
     *  Normalizes the index backwards with respect to the given offset.
     * @param index Index.
     * @param offset Offset to index.
     * @return Normalized index.
     */
    static std::uint16_t normalize_backward(const std::uint16_t index, const std::uint16_t offset) noexcept
    {
        return index >= offset ? index - offset : S - (offset - index);
    }
};

template <std::size_t S> TaskInterface *TaskBuffer<S>::next() noexcept
{
    auto &slot = this->_buffer[this->_head];
    if (slot != nullptr)
    {
        slot.prefetch();
        this->_head = TaskBuffer<S>::normalize(this->_head + 1U);
        return slot.consume_task();
    }

    return nullptr;
}

template <std::size_t S>
template <class Q>
std::uint16_t TaskBuffer<S>::fill(Q &from_queue, const std::uint16_t count) noexcept
{
    if (count == 0U || from_queue.empty())
    {
        return 0U;
    }

    const auto size = S - count;
    const auto is_prefetching = this->_prefetch_distance > 0U;
    auto prefetch_tail = TaskBuffer<S>::normalize_backward(this->_tail, this->_prefetch_distance);

    for (auto i = 0U; i < count; ++i)
    {
        auto *task = static_cast<TaskInterface *>(from_queue.pop_front());
        if (task == nullptr)
        {
            return i;
        }

        // Schedule prefetch instruction <prefetch_distance> slots before.
        if (is_prefetching && (size + i) >= this->_prefetch_distance)
        {
            this->_buffer[prefetch_tail].prefetch(task);
        }

        // Schedule task.
        this->_buffer[this->_tail].task(task);

        // Increment tail.
        this->_tail = TaskBuffer<S>::normalize(this->_tail + 1U);
        prefetch_tail = TaskBuffer<S>::normalize(prefetch_tail + 1U);
    }

    return count;
}
} // namespace mx::tasking