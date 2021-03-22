#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mx/memory/global_heap.h>
#include <mx/system/builtin.h>

namespace mx::util {
/**
 * Multi producer, multi consumer queue with a fixed number of slots.
 * Every thread can push and pop values into the queue without using latches.
 *
 * Inspired by http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 */
template <typename T> class BoundMPMCQueue
{
public:
    BoundMPMCQueue(const std::uint16_t capacity) noexcept : _capacity(capacity)
    {
        _storage =
            new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(std::pair<std::atomic_uint64_t, T>) * capacity))
                std::pair<std::atomic_uint64_t, T>[capacity];
        std::memset(static_cast<void *>(_storage), 0, sizeof(std::pair<std::atomic_uint64_t, T>) * capacity);

        for (auto i = 0U; i < capacity; ++i)
        {
            std::get<0>(_storage[i]).store(i, std::memory_order_relaxed);
        }
    }

    ~BoundMPMCQueue() noexcept { delete[] _storage; }

    BoundMPMCQueue(const BoundMPMCQueue<T> &) = delete;
    BoundMPMCQueue(BoundMPMCQueue<T> &&) = delete;

    BoundMPMCQueue<T> &operator=(const BoundMPMCQueue<T> &) = delete;
    BoundMPMCQueue<T> &operator=(BoundMPMCQueue<T> &&) = delete;

    /**
     * Inserts the given value.
     * May block until a slot is available.
     * @param item Data to insert.
     */
    void push_back(const T &item) noexcept
    {
        while (try_push_back(item) == false)
        {
            system::builtin::pause();
        }
    }

    /**
     * Takes out the next value.
     * May block until data is available.
     * @return The popped value.
     */
    T pop_front() noexcept
    {
        T item;
        while (try_pop_front(item) == false)
        {
            system::builtin::pause();
        }
        return item;
    }

    /**
     * Tries to take out the next value or the given default value,
     * if no data is available.
     * @param default_value Data that will be returned if no data is available.
     * @return Popped data or default value.
     */
    T pop_front_or(const T &default_value) noexcept
    {
        T item;
        if (try_pop_front(item))
        {
            return item;
        }
        else
        {
            return default_value;
        }
    }

    /**
     * Tries to insert value into the queue.
     * @param item Item to insert.
     * @return True, when successful inserted; false if no slot was available.
     */
    bool try_push_back(const T &item) noexcept
    {
        auto pos = _head.load(std::memory_order_relaxed);
        std::uint64_t slot;
        for (;;)
        {
            slot = pos % _capacity;
            const auto sequence = std::get<0>(_storage[slot]).load(std::memory_order_acquire);
            const auto difference = std::int64_t(sequence) - std::int64_t(pos);
            if (difference == 0)
            {
                if (_head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                return false;
            }
            else
            {
                pos = _head.load(std::memory_order_relaxed);
            }
        }

        std::get<1>(_storage[slot]) = item;
        std::get<0>(_storage[slot]).store(pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * Tries to take the next value.
     * @param return_item Item where the next value will be stored.
     * @return True, when pop was successful; false if no data was available.
     */
    bool try_pop_front(T &return_item) noexcept
    {
        auto pos = _tail.load(std::memory_order_relaxed);
        std::uint64_t slot;
        for (;;)
        {
            slot = pos % _capacity;
            const auto sequence = std::get<0>(_storage[slot]).load(std::memory_order_acquire);
            const auto difference = std::int64_t(sequence) - std::int64_t(pos + 1);
            if (difference == 0)
            {
                if (_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                return false;
            }
            else
            {
                pos = _tail.load(std::memory_order_relaxed);
            }
        }

        return_item = std::get<1>(_storage[slot]);
        std::get<0>(_storage[slot]).store(pos + _capacity, std::memory_order_release);
        return true;
    }

private:
    // Capacity of the queue.
    const std::uint32_t _capacity;

    // Array of status flags and data slots.
    std::pair<std::atomic_uint64_t, T> *_storage;

    // Index of the head.
    alignas(64) std::atomic_uint64_t _head{0U};

    // Index of the tail.
    alignas(64) std::atomic_uint64_t _tail{0U};
};
} // namespace mx::util