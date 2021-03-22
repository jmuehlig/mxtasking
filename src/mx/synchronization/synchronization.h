#pragma once
#include <cstdint>

namespace mx::synchronization {
/**
 * Desired isolation level of a resource.
 */
enum class isolation_level : std::uint8_t
{
    ExclusiveWriter = 0U, // Reads can be parallel, writes will be synchronized
    Exclusive = 1U,       // All accesses will be synchronized
    None = 2U,            // Nothing will be synchronized
};

/**
 * Desired protocol of synchronization.
 */
enum class protocol : std::uint8_t
{
    None = 0U,               // System is free to choose
    Queue = 1U,              // Choose primitive with queues with respect to isolation level
    Latch = 2U,              // Choose primitive with latches with respect to isolation level
    OLFIT = 3U,              // Try to choose olfit
    TransactionalMemory = 4U // Try to choose htm
};

/**
 * Real method, based on the isolation level
 * and decision by the tasking layer.
 *
 * Attention: Even if the primitive is 8bit long,
 *            it is stored within the tagged_ptr as
 *            using only 4bit! Therefore, the max.
 *            value can be 15.
 */
enum class primitive : std::uint8_t
{
    None = 0U,              // Nothing will be synchronized
    ExclusiveLatch = 1U,    // All accesses will use a spinlock
    ScheduleAll = 2U,       // All accesses will be scheduled to the mapped channel
    ReaderWriterLatch = 3U, // Use a reader/writer latch to enable parallel reads
    ScheduleWriter = 4U,    // Reads can perform anywhere, writes are scheduled to the mapped channel
    OLFIT = 5U              // Read/write anywhere but use a latch for writers
};

/**
 * Checks whether the given primitive is kind of optimistic synchronization
 * or not.
 * @param primitive_ Primitive to check.
 * @return True, if the given primitive is optimistic.
 */
static inline bool is_optimistic(const primitive primitive_) noexcept
{
    return primitive_ == primitive::ScheduleWriter || primitive_ == primitive::OLFIT;
}

} // namespace mx::synchronization