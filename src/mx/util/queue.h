#pragma once

namespace mx::util {
/**
 * Single producer, single consumer queue. This queue is not thread safe.
 */
template <class T> class Queue
{
public:
    constexpr Queue() noexcept = default;
    ~Queue() noexcept = default;

    /**
     * Inserts an item into the queue.
     * @param item Item to be inserted.
     */
    void push_back(T *item) noexcept
    {
        item->next(nullptr);

        if (_tail != nullptr)
        {
            _tail->next(item);
            _tail = item;
        }
        else
        {
            _head = _tail = item;
        }
    }

    /**
     * @return Begin of the queue.
     */
    [[nodiscard]] T *begin() noexcept { return _head; }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] const T *end() const noexcept { return _tail; }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] T *end() noexcept { return _tail; }

    /**
     * @return True, when the queue is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _head == nullptr; }

    /**
     * @return Takes and removes the first item from the queue.
     */
    T *pop_front() noexcept
    {
        if (_head == nullptr)
        {
            return nullptr;
        }

        auto *head = _head;
        auto *new_head = head->next();
        if (new_head == nullptr)
        {
            _tail = nullptr;
        }

        _head = new_head;
        return head;
    }

private:
    // Pointer to the head.
    alignas(64) T *_head{nullptr};

    // Pointer to the tail.
    T *_tail{nullptr};
};
} // namespace mx::util