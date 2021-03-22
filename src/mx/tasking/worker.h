#pragma once

#include "channel.h"
#include "config.h"
#include "profiling/statistic.h"
#include "task.h"
#include "task_stack.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mx/memory/reclamation/epoch_manager.h>
#include <mx/util/maybe_atomic.h>
#include <variant>
#include <vector>

namespace mx::tasking {
/**
 * The worker executes tasks from his own channel, until the "running" flag is false.
 */
class alignas(64) Worker
{
public:
    Worker(std::uint16_t id, std::uint16_t target_core_id, std::uint16_t target_numa_node_id,
           const util::maybe_atomic<bool> &is_running, std::uint16_t prefetch_distance,
           memory::reclamation::LocalEpoch &local_epoch, const std::atomic<memory::reclamation::epoch_t> &global_epoch,
           profiling::Statistic &statistic) noexcept;

    ~Worker() noexcept = default;

    /**
     * Starts the worker (typically in its own thread).
     */
    void execute();

    /**
     * @return Id of the logical core this worker runs on.
     */
    [[nodiscard]] std::uint16_t core_id() const noexcept { return _target_core_id; }

    [[nodiscard]] Channel &channel() noexcept { return _channel; }
    [[nodiscard]] const Channel &channel() const noexcept { return _channel; }

private:
    // Id of the logical core.
    const std::uint16_t _target_core_id;

    // Distance of prefetching tasks.
    const std::uint16_t _prefetch_distance;

    std::int32_t _channel_size{0U};

    // Stack for persisting tasks in optimistic execution. Optimistically
    // executed tasks may fail and be restored after execution.
    alignas(64) TaskStack _task_stack;

    // Channel where tasks are stored for execution.
    alignas(64) Channel _channel;

    // Local epoch of this worker.
    memory::reclamation::LocalEpoch &_local_epoch;

    // Global epoch.
    const std::atomic<memory::reclamation::epoch_t> &_global_epoch;

    // Statistics container.
    profiling::Statistic &_statistic;

    // Flag for "running" state of MxTasking.
    const util::maybe_atomic<bool> &_is_running;

    /**
     * Analyzes the given task and chooses the execution method regarding synchronization.
     * @param task Task to be executed.
     * @return Synchronization method.
     */
    static synchronization::primitive synchronization_primitive(TaskInterface *task) noexcept
    {
        return task->has_resource_annotated() ? task->annotated_resource().synchronization_primitive()
                                              : synchronization::primitive::None;
    }

    /**
     * Executes a task with a latch.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_exclusive_latched(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes a task with a reader/writer latch.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_reader_writer_latched(std::uint16_t core_id, std::uint16_t channel_id,
                                                    TaskInterface *task);

    /**
     * Executes the task optimistically.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes the task using olfit protocol.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_olfit(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes the read-only task optimistically.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param resource Resource the task reads.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic_read(std::uint16_t core_id, std::uint16_t channel_id,
                                       resource::ResourceInterface *resource, TaskInterface *task);
};
} // namespace mx::tasking