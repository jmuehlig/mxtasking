#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mx/system/cache.h>

namespace mx::util {
/**
 * Multi producer, single consumer queue with unlimited slots.
 * Every thread can push values into the queue without using latches.
 *
 * Inspired by http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
 */
template <class T> class MPSCQueue
{
public:
    constexpr MPSCQueue() noexcept
        : _head(reinterpret_cast<T *>(&_stub)), _tail(reinterpret_cast<T *>(&_stub)),
          _end(reinterpret_cast<T *>(&_stub))
    {
        reinterpret_cast<T &>(_stub).next(nullptr);
    }
    ~MPSCQueue() noexcept = default;

    /**
     * Inserts the given item into the queue.
     * @param item Item to insert.
     */
    void push_back(T *item) noexcept
    {
        item->next(nullptr);
        auto *prev = __atomic_exchange_n(&_head, item, __ATOMIC_RELAXED);
        prev->next(item);
    }

    /**
     * Inserts all items between begin and end into the queue.
     * Items must be linked among themselves.
     * @param begin First item to insert.
     * @param end Last item to insert.
     */
    void push_back(T *begin, T *end) noexcept
    {
        end->next(nullptr);
        auto *old_head = __atomic_exchange_n(&_head, end, __ATOMIC_RELAXED);
        old_head->next(begin);
    }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] const T *end() const noexcept { return _end; }

    /**
     * @return True, when the queue is empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return _tail == _end && reinterpret_cast<T const &>(_stub).next() == nullptr;
    }

    /**
     * @return Takes and removes the first item from the queue.
     */
    T *pop_front() noexcept;

private:
    // Head of the queue (accessed by every producer).
    alignas(64) T *_head;

    // Tail of the queue (accessed by the consumer and producers if queue is empty)-
    alignas(64) T *_tail;

    // Pointer to the end.
    alignas(16) T *const _end;

    // Dummy item for empty queue.
    alignas(64) std::array<std::byte, sizeof(T)> _stub;
};

template <class T> T *MPSCQueue<T>::pop_front() noexcept
{
    auto *tail = this->_tail;
    auto *next = tail->next();

    if (tail == this->_end)
    {
        if (next == nullptr)
        {
            return nullptr;
        }

        this->_tail = next;
        tail = next;
        next = next->next();
    }

    if (next != nullptr)
    {
        this->_tail = next;
        return tail;
    }

    const auto *head = this->_head;
    if (tail != head)
    {
        return nullptr;
    }

    this->push_back(this->_end);

    next = tail->next();
    if (next != nullptr)
    {
        this->_tail = next;
        return tail;
    }

    return nullptr;
}
} // namespace mx::util