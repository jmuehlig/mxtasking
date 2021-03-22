#pragma once

#include <atomic>
#include <cstdint>
#include <mx/system/builtin.h>

namespace mx::synchronization {
/**
 * Simple spinlock for mutual exclusion.
 */
class Spinlock
{
public:
    constexpr Spinlock() noexcept = default;
    ~Spinlock() = default;

    /**
     * Locks the spinlock by spinning until it is lockable.
     */
    void lock() noexcept
    {
        while (true)
        {
            while (_flag.load(std::memory_order_relaxed))
            {
                system::builtin::pause();
            }

            if (try_lock())
            {
                return;
            }
        }
    }

    /**
     * Try to lock the lock.
     * @return True, when successfully locked.
     */
    bool try_lock() noexcept
    {
        bool expected = false;
        return _flag.compare_exchange_weak(expected, true, std::memory_order_acquire);
    }

    /**
     * Unlocks the spinlock.
     */
    void unlock() noexcept { _flag.store(false, std::memory_order_acquire); }

    /**
     * @return True, if the lock is in use.
     */
    [[nodiscard]] bool is_locked() const noexcept { return _flag.load(std::memory_order_relaxed); }

private:
    std::atomic_bool _flag{false};
};
} // namespace mx::synchronization
