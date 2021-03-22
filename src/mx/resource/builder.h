#pragma once
#include "resource.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <mx/memory/dynamic_size_allocator.h>
#include <mx/memory/global_heap.h>
#include <mx/tasking/config.h>
#include <mx/tasking/scheduler.h>
#include <mx/util/aligned_t.h>
#include <type_traits>
#include <utility>

namespace mx::resource {
/**
 * The Builder constructs and deletes data objects.
 * Besides, the Builder schedules data objects to
 * channels.
 */
class Builder
{
public:
    Builder(tasking::Scheduler &scheduler, memory::dynamic::Allocator &allocator) noexcept
        : _allocator(allocator), _scheduler(scheduler)
    {
    }

    ~Builder() noexcept = default;

    /**
     * Build a data object of given type with given
     * size and arguments. The hint defines the synchronization
     * requirements and affects scheduling.
     *
     * @param size Size of the data object.
     * @param hint  Hint for scheduling and synchronization.
     * @param arguments Arguments to the constructor.
     * @return Tagged pointer holding the synchronization, assigned channel and pointer.
     */
    template <typename T, typename... Args>
    ptr build(const std::size_t size, resource::hint &&hint, Args &&... arguments) noexcept
    {
#ifndef NDEBUG
        if (hint != synchronization::isolation_level::None &&
            (hint != synchronization::isolation_level::Exclusive || hint != synchronization::protocol::Queue))
        {
            if constexpr (std::is_base_of<ResourceInterface, T>::value == false)
            {
                assert(false && "Type must be inherited from mx::resource::ResourceInterface");
            }
        }
#endif

        const auto synchronization_method = Builder::isolation_level_to_synchronization_primitive(hint);

        const auto [channel_id, numa_node_id] = schedule(hint);
        const auto resource_information = information{channel_id, synchronization_method};

        return ptr{new (_allocator.allocate(numa_node_id, 64U, size)) T(std::forward<Args>(arguments)...),
                   resource_information};
    }

    /**
     * Builds data resourced from an existing pointer.
     * The hint defines the synchronization
     * requirements and affects scheduling.
     * @param object
     * @param hint  Hint for scheduling and synchronization.
     * @return Tagged pointer holding the synchronization, assigned channel and pointer.
     */
    template <typename T> ptr build(T *object, resource::hint &&hint) noexcept
    {
#ifndef NDEBUG
        if (hint != synchronization::isolation_level::None &&
            (hint != synchronization::isolation_level::Exclusive || hint != synchronization::protocol::Queue))
        {
            if constexpr (std::is_base_of<ResourceInterface, T>::value == false)
            {
                assert(false && "Type must be inherited from mx::resource::ResourceInterface");
            }
        }
#endif

        const auto synchronization_method = Builder::isolation_level_to_synchronization_primitive(hint);
        const auto [channel_id, _] = schedule(hint);

        return ptr{object, information{channel_id, synchronization_method}};
    }

    /**
     * Destroys the given data object.
     * @param core_id Executing core.
     * @param resource Tagged pointer to the data object.
     */
    template <typename T> void destroy(const ptr resource)
    {
        // TODO: Revoke usage prediction?
        if (resource != nullptr)
        {
            if constexpr (tasking::config::memory_reclamation() != tasking::config::None)
            {
                if (synchronization::is_optimistic(resource.synchronization_primitive()))
                {
                    _scheduler.epoch_manager().add_to_garbage_collection(resource.get<resource::ResourceInterface>(),
                                                                         resource.channel_id());
                    return;
                }
            }

            // No need to reclaim memory.
            resource.get<T>()->~T();
            _allocator.free(resource.get<void>());
        }
    }

private:
    // Internal allocator for dynamic sized allocation.
    memory::dynamic::Allocator &_allocator;

    // Scheduler of MxTasking to get access to channels.
    tasking::Scheduler &_scheduler;

    // Next channel id for round-robin scheduling.
    alignas(64) std::atomic_uint16_t _round_robin_channel_id{0U};

    /**
     * Schedules the resource to a channel, affected by the given hint.
     *
     * @param hint Hint for scheduling.
     * @return Pair of Channel and NUMA node IDs.
     */
    std::pair<std::uint16_t, std::uint8_t> schedule(const resource::hint &hint);

    /**
     * Determines the best synchronization method based on
     * synchronization requirement.
     *
     * @param isolation_level Synchronization requirement.
     * @param prefer_latch Prefer latch for synchronization or latch-free?
     * @return Chosen synchronization method.
     */
    static synchronization::primitive isolation_level_to_synchronization_primitive(const hint &hint) noexcept;
};
} // namespace mx::resource