#pragma once

#include <atomic>
#include <cstdint>

namespace mx::util {
/**
 * Counter for counting references to an object.
 *
 * Inspired by
 *  "Pay Migration Tax to Homeland: Anchor-based Scalable Reference Counting for Multicores"
 *   17th USENIX Conference on File and Storage Technologies (FAST ’19)
 *   https://www.usenix.org/system/files/fast19-jung.pdf
 */
template <typename T> class reference_counter
{
public:
    explicit constexpr reference_counter(const std::uint16_t core_id) noexcept : _local_core_id(core_id) {}

    ~reference_counter() noexcept = default;

    /**
     * Increases the counter.
     * @param core_id Logical core identifier of the caller.
     * @param count Number of increase.
     */
    void add(const std::uint16_t core_id, const T count = 1) noexcept
    {
        if (is_local(core_id))
        {
            _local_counter += count;
        }
        else
        {
            _remote_counter.fetch_add(count, std::memory_order_relaxed);
        }
    }

    /**
     * Decreases the counter.
     * @param core_id Logical core identifier of the caller.
     * @param count Number of decrease.
     */
    void sub(const std::uint16_t core_id, const T count = 1) noexcept
    {
        if (is_local(core_id))
        {
            _local_counter -= count;
        }
        else
        {
            _remote_counter.fetch_sub(count, std::memory_order_relaxed);
        }
    }

    /**
     * @return The current stored counter.
     */
    [[nodiscard]] T load() const noexcept { return _local_counter + _remote_counter.load(std::memory_order_relaxed); }

    /**
     * @param core_id Logical core identifier of caller.
     * @return True, if the counter is local to the calling core.
     */
    [[nodiscard]] bool is_local(const std::uint16_t core_id) const noexcept { return _local_core_id == core_id; }

private:
    // Identifier of the local core.
    const std::uint16_t _local_core_id;

    // Local core counter.
    T _local_counter{0};

    // Counter of all other cores.
    std::atomic<T> _remote_counter{0};
};

using reference_counter_16 = reference_counter<std::int16_t>;
using reference_counter_32 = reference_counter<std::int32_t>;
using reference_counter_64 = reference_counter<std::int64_t>;
} // namespace mx::util