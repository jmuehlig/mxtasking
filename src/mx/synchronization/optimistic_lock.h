#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <mx/system/builtin.h>
#include <mx/tasking/config.h>

namespace mx::synchronization {
class OptimisticLock
{
public:
    using version_t = std::uint32_t;

    constexpr OptimisticLock() = default;
    ~OptimisticLock() = default;

    /**
     * Guarantees to read a valid version by blocking until
     * the version is not locked.
     * @return The current version.
     */
    [[nodiscard]] version_t read_valid() const noexcept
    {
        auto version = _version.load(std::memory_order_seq_cst);
        while (OptimisticLock::is_locked(version))
        {
            system::builtin::pause();
            version = _version.load(std::memory_order_seq_cst);
        }
        return version;
    }

    /**
     * Validates the version.
     *
     * @param version The version to validate.
     * @return True, if the version is valid.
     */
    [[nodiscard]] bool is_valid(const version_t version) const noexcept
    {
        return version == _version.load(std::memory_order_seq_cst);
    }

    /**
     * Tries to acquire the lock.
     * @return True, when lock was acquired.
     */
    [[nodiscard]] bool try_lock() noexcept
    {
        auto version = read_valid();

        return _version.compare_exchange_strong(version, version + 0b10);
    }

    /**
     * Waits until the lock is successfully acquired.
     */
    template <bool SINGLE_WRITER> void lock() noexcept
    {
        if constexpr (SINGLE_WRITER)
        {
            _version.fetch_add(0b10, std::memory_order_seq_cst);
        }
        else
        {
            auto tries = std::uint64_t{1U};
            while (this->try_lock() == false)
            {
                const auto wait = tries++;
                for (auto i = 0U; i < wait * 32U; ++i)
                {
                    system::builtin::pause();
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
            }
        }
    }

    /**
     * Unlocks the version lock.
     */
    void unlock() noexcept { _version.fetch_add(0b10, std::memory_order_seq_cst); }

private:
    std::atomic<version_t> _version{0b100};

    [[nodiscard]] static bool is_locked(const version_t version) noexcept { return (version & 0b10) == 0b10; }
};
} // namespace mx::synchronization