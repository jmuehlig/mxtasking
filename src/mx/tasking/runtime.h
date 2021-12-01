#pragma once
#include "scheduler.h"
#include "task.h"
#include <iostream>
#include <memory>
#include <mx/memory/dynamic_size_allocator.h>
#include <mx/memory/fixed_size_allocator.h>
#include <mx/memory/task_allocator_interface.h>
#include <mx/resource/builder.h>
#include <mx/util/core_set.h>
#include <utility>

namespace mx::tasking {
/**
 * The runtime is the central access structure to MxTasking.
 * Here, we can initialize MxTasking, spawn and allocate tasks, allocate
 * data objects.
 */
class runtime
{
public:
    /**
     * Initializes the MxTasking runtime.
     * @param core_set Cores, where the runtime should execute on.
     * @param prefetch_distance Distance for prefetching.
     * @param channels_per_core Number of channels per core (more than one enables channel-stealing).
     * @param use_system_allocator Should we use the systems malloc interface or our allocator?
     * @return True, when the runtime was started successfully.
     */
    static bool init(const util::core_set &core_set, const std::uint16_t prefetch_distance,
                     const bool use_system_allocator)
    {
        // Are we ready to re-initialize the scheduler?
        if (_scheduler != nullptr && _scheduler->is_running())
        {
            return false;
        }

        // Create a new resource allocator.
        if (_resource_allocator == nullptr)
        {
            _resource_allocator.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(
                sizeof(memory::dynamic::Allocator))) memory::dynamic::Allocator());
        }
        else if (_resource_allocator->is_free())
        {
            _resource_allocator->release_allocated_memory();
            _resource_allocator->initialize_empty();
        }
        else
        {
            _resource_allocator->defragment();
        }

        // Create a new task allocator.
        if (use_system_allocator)
        {
            _task_allocator.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(
                memory::SystemTaskAllocator<config::task_size()>))) memory::SystemTaskAllocator<config::task_size()>());
        }
        else
        {
            _task_allocator.reset(new (
                memory::GlobalHeap::allocate_cache_line_aligned(sizeof(memory::fixed::Allocator<config::task_size()>)))
                                      memory::fixed::Allocator<config::task_size()>(core_set));
        }

        // Create a new scheduler.
        const auto need_new_scheduler = _scheduler == nullptr || *_scheduler != core_set;
        if (need_new_scheduler)
        {
            _scheduler.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(Scheduler)))
                                 Scheduler(core_set, prefetch_distance, *_resource_allocator));
        }
        else
        {
            _scheduler->reset();
        }

        // Create a new resource builder.
        if (_resource_builder == nullptr || need_new_scheduler)
        {
            _resource_builder = std::make_unique<resource::Builder>(*_scheduler, *_resource_allocator);
        }

        return true;
    }

    /**
     * Start profiling of idle times. Results will be written to the given file.
     * @param output_file File for idle-time results.
     */
    static void profile(const std::string &output_file) noexcept { _scheduler->profile(output_file); }

    /**
     * Spawns the given task.
     * @param task Task to be scheduled.
     * @param current_channel_id Channel, the spawn request came from.
     */
    static void spawn(TaskInterface &task, const std::uint16_t current_channel_id) noexcept
    {
        _scheduler->schedule(task, current_channel_id);
    }

    /**
     * Spawns the given task.
     * @param task Task to be scheduled.
     */
    static void spawn(TaskInterface &task) noexcept { _scheduler->schedule(task); }

    /**
     * @return Number of available channels.
     */
    static std::uint16_t channels() noexcept { return _scheduler->count_channels(); }

    /**
     * Starts the runtime and suspends the starting thread until MxTasking is stopped.
     */
    static void start_and_wait() { _scheduler->start_and_wait(); }

    /**
     * Instructs all worker threads to stop their work.
     * After all worker threads are stopped, the starting
     * thread will be resumed.
     */
    static void stop() noexcept { _scheduler->interrupt(); }

    /**
     * Creates a new task.
     * @param core_id Core to allocate memory from.
     * @param arguments Arguments for the task.
     * @return The new task.
     */
    template <typename T, typename... Args> static T *new_task(const std::uint16_t core_id, Args &&... arguments)
    {
        static_assert(sizeof(T) <= config::task_size() && "Task must be leq defined task size.");
        return new (_task_allocator->allocate(core_id)) T(std::forward<Args>(arguments)...);
    }

    /**
     * Frees a given task.
     * @param core_id Core id to return the memory to.
     * @param task Task to be freed.
     */
    template <typename T> static void delete_task(const std::uint16_t core_id, T *task) noexcept
    {
        task->~T();
        _task_allocator->free(core_id, static_cast<void *>(task));
    }

    /**
     * Creates a resource.
     * @param size Size of the data object.
     * @param hint Hints for allocation and scheduling.
     * @param arguments Arguments for the data object.
     * @return The resource pointer.
     */
    template <typename T, typename... Args>
    static resource::ptr new_resource(const std::size_t size, resource::hint &&hint, Args &&... arguments) noexcept
    {
        return _resource_builder->build<T>(size, std::move(hint), std::forward<Args>(arguments)...);
    }

    /**
     * Creates a resource from a given pointer.
     * @param object Pointer to the existing object.
     * @param hint Hints for allocation and scheduling.
     * @return The resource pointer.
     */
    template <typename T> static resource::ptr to_resource(T *object, resource::hint &&hint) noexcept
    {
        return _resource_builder->build<T>(object, std::move(hint));
    }

    /**
     * Deletes the given data object.
     * @param resource Data object to be deleted.
     */
    template <typename T> static void delete_resource(const resource::ptr resource) noexcept
    {
        _resource_builder->destroy<T>(resource);
    }

    static void *allocate(const std::uint8_t numa_node_id, const std::size_t alignment, const std::size_t size) noexcept
    {
        return _resource_allocator->allocate(numa_node_id, alignment, size);
    }

    static void free(void *pointer) noexcept { _resource_allocator->free(pointer); }

    /**
     * Updates the prediction of a data object.
     * @param resource Data object, whose usage should be predicted.
     * @param old_prediction Prediction so far.
     * @param new_prediction New usage prediction.
     */
    static void modify_predicted_usage(const resource::ptr resource,
                                       const resource::hint::expected_access_frequency old_prediction,
                                       const resource::hint::expected_access_frequency new_prediction) noexcept
    {
        _scheduler->modify_predicted_usage(resource.channel_id(), old_prediction, new_prediction);
    }

    /**
     * ID of the NUMA region of a channel.
     * @param channel_id Channel.
     * @return ID of the NUMA region.
     */
    static std::uint8_t numa_node_id(const std::uint16_t channel_id) noexcept
    {
        return _scheduler->numa_node_id(channel_id);
    }

    /**
     * Reads the task statistics for a given counter and all channels.
     * @param counter Counter to be read.
     * @return Aggregated value of all channels.
     */
    static std::uint64_t statistic(const profiling::Statistic::Counter counter) noexcept
    {
        return _scheduler->statistic(counter);
    }

    /**
     * Reads the task statistic for a given counter on a given channel.
     * @param counter Counter to be read.
     * @param channel_id Channel.
     * @return Value of the counter of the given channel.
     */
    static std::uint64_t statistic(const profiling::Statistic::Counter counter, const std::uint16_t channel_id) noexcept
    {
        return _scheduler->statistic(counter, channel_id);
    }

private:
    // Scheduler to spawn tasks.
    inline static std::unique_ptr<Scheduler> _scheduler = {nullptr};

    // Allocator to allocate tasks (could be systems malloc or our Multi-level allocator).
    inline static std::unique_ptr<memory::TaskAllocatorInterface> _task_allocator = {nullptr};

    // Allocator to allocate resources.
    inline static std::unique_ptr<memory::dynamic::Allocator> _resource_allocator = {nullptr};

    // Allocator to allocate data objects.
    inline static std::unique_ptr<resource::Builder> _resource_builder = {nullptr};
};

/**
 * The runtime_guard initializes the runtime at initialization and starts
 * the runtime when the object is deleted. This allows MxTasking to execute
 * within a specific scope.
 */
class runtime_guard
{
public:
    runtime_guard(const bool use_system_allocator, const util::core_set &core_set,
                  const std::uint16_t prefetch_distance = 0U) noexcept
    {
        runtime::init(core_set, prefetch_distance, use_system_allocator);
    }

    runtime_guard(const util::core_set &core_set, const std::uint16_t prefetch_distance = 0U) noexcept
        : runtime_guard(false, core_set, prefetch_distance)
    {
    }

    ~runtime_guard() noexcept { runtime::start_and_wait(); }
};
} // namespace mx::tasking