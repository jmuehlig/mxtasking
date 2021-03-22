#pragma once

#include "epoch_t.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mx/memory/config.h>
#include <mx/memory/dynamic_size_allocator.h>
#include <mx/resource/resource_interface.h>
#include <mx/system/builtin.h>
#include <mx/tasking/config.h>
#include <mx/tasking/task.h>
#include <mx/util/aligned_t.h>
#include <mx/util/core_set.h>
#include <mx/util/maybe_atomic.h>
#include <mx/util/mpsc_queue.h>
#include <thread>

namespace mx::memory::reclamation {
class alignas(64) LocalEpoch
{
public:
    constexpr LocalEpoch() noexcept = default;
    ~LocalEpoch() noexcept = default;

    LocalEpoch &operator=(const epoch_t epoch) noexcept
    {
        _epoch = epoch;
        return *this;
    }

    void enter(const std::atomic<epoch_t> &global_epoch) noexcept
    {
        _epoch.store(global_epoch.load(std::memory_order_seq_cst), std::memory_order_seq_cst);
    }
    void leave() noexcept { _epoch.store(std::numeric_limits<epoch_t>::max()); }

    [[nodiscard]] epoch_t operator()() const noexcept { return _epoch.load(std::memory_order_seq_cst); }

private:
    std::atomic<epoch_t> _epoch{std::numeric_limits<epoch_t>::max()};
};

/**
 * The Epoch Manager manages periodic epochs which
 * are used to protect reads against concurrent
 * delete operations. Therefore, a global epoch
 * will be incremented every 50ms (configurable).
 * Read operations, on the other hand, will update
 * their local epoch every time before reading an
 * optimistic resource.
 * When (logically) deleting an optimistic resource,
 * the resource will be deleted physically, when
 * every local epoch is greater than the epoch
 * when the resource is deleted.
 */
class EpochManager
{
public:
    EpochManager(const std::uint16_t count_channels, dynamic::Allocator &allocator,
                 util::maybe_atomic<bool> &is_running) noexcept
        : _count_channels(count_channels), _is_running(is_running), _allocator(allocator)
    {
    }

    EpochManager(const EpochManager &) = delete;

    ~EpochManager() = default;

    LocalEpoch &operator[](const std::uint16_t channel_id) noexcept { return _local_epochs[channel_id]; }

    /**
     * @return Access to read to global epoch.
     */
    [[nodiscard]] const std::atomic<epoch_t> &global_epoch() const noexcept { return _global_epoch; }

    /**
     * @return The minimal epoch of all channels.
     */
    [[nodiscard]] epoch_t min_local_epoch() const noexcept
    {
        auto min_epoch = _local_epochs[0U]();
        for (auto channel_id = 1U; channel_id < _count_channels; ++channel_id)
        {
            min_epoch = std::min(min_epoch, _local_epochs[channel_id]());
        }

        return min_epoch;
    }

    /**
     * Adds an optimistic resource to garbage collection.
     * @param resource Resource to logically delete.
     */
    void add_to_garbage_collection(resource::ResourceInterface *resource,
                                   [[maybe_unused]] const std::uint16_t owning_channel_id) noexcept
    {
        resource->remove_epoch(_global_epoch.load(std::memory_order_acq_rel));

        if constexpr (config::local_garbage_collection())
        {
            _local_garbage_queues[owning_channel_id].value().push_back(resource);
        }
        else
        {
            _global_garbage_queue.push_back(resource);
        }
    }

    /**
     * Called periodically by a separate thread.
     */
    void enter_epoch_periodically();

    /**
     * Reclaims all garbage, mainly right before shut down tasking.
     */
    void reclaim_all() noexcept;

    /**
     * Grants access to the local garbage queue of a specific channel.
     *
     * @param channel_id Channel Id.
     * @return Local garbage queue.
     */
    [[nodiscard]] util::MPSCQueue<resource::ResourceInterface> &local_garbage(const std::uint16_t channel_id) noexcept
    {
        return _local_garbage_queues[channel_id].value();
    }

    /**
     * Reset all local and the global epoch to initial values
     * if no memory is in use.
     */
    void reset() noexcept;

private:
    // Number of used channels; important for min-calculation.
    const std::uint16_t _count_channels;

    // Flag of the scheduler indicating the state of the system.
    util::maybe_atomic<bool> &_is_running;

    // Allocator to free collected resources.
    dynamic::Allocator &_allocator;

    // Global epoch, incremented periodically.
    std::atomic<epoch_t> _global_epoch{0U};

    // Local epochs, one for every channel.
    alignas(64) std::array<LocalEpoch, tasking::config::max_cores()> _local_epochs;

    // Queue that holds all logically deleted objects in a global space.
    alignas(64) util::MPSCQueue<resource::ResourceInterface> _global_garbage_queue;

    // Queues for every worker thread. Logically deleted objects are stored here
    // whenever local garbage collection is used.
    alignas(64) std::array<util::aligned_t<util::MPSCQueue<resource::ResourceInterface>>,
                           tasking::config::max_cores()> _local_garbage_queues;

    /**
     * Reclaims resources with regard to the epoch.
     */
    void reclaim_epoch_garbage() noexcept;
};

class ReclaimEpochGarbageTask final : public tasking::TaskInterface
{
public:
    constexpr ReclaimEpochGarbageTask(EpochManager &epoch_manager, dynamic::Allocator &allocator) noexcept
        : _epoch_manager(epoch_manager), _allocator(allocator)
    {
    }
    ~ReclaimEpochGarbageTask() noexcept override = default;

    tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

private:
    EpochManager &_epoch_manager;
    dynamic::Allocator &_allocator;
};
} // namespace mx::memory::reclamation