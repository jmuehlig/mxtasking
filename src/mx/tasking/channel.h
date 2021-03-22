#pragma once

#include "channel_occupancy.h"
#include "load.h"
#include "task.h"
#include "task_buffer.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <mx/memory/config.h>
#include <mx/system/cache.h>
#include <mx/util/mpsc_queue.h>
#include <mx/util/queue.h>

namespace mx::tasking {
/**
 * The channel is the central data structure where tasks are scheduled to pass tasks
 * between worker threads. Every worker thread owns his own channel, where tasks
 * are popped by only this channel. Every worker thread (or task) can push further
 * tasks to the channel.
 *
 * Every channel consists of a handful of queues, where the tasks are really stored,
 * different queues have different guarantees regarding concurrency and locality.
 *
 * In addition, every channel has its own buffer, where tasks are transferred from the
 * queues. If the buffer is empty, the worker thread will fill it with tasks from backend
 * queues.
 *
 * The buffer enables the worker thread to have a view to tasks that are ready for execution;
 * this is used e.g. for prefetching.
 */
class Channel
{
public:
    constexpr Channel(const std::uint16_t id, const std::uint8_t numa_node_id,
                      const std::uint8_t prefetch_distance) noexcept
        : _remote_queues({}), _local_queues({}), _task_buffer(prefetch_distance), _id(id), _numa_node_id(numa_node_id)
    {
    }
    ~Channel() noexcept = default;

    /**
     * @return Identifier of the channel.
     */
    [[nodiscard]] std::uint16_t id() const noexcept { return _id; }

    /**
     * @return The next task to be executed.
     */
    TaskInterface *next() noexcept { return _task_buffer.next(); }

    /**
     * Schedules the task to thread-safe queue with regard to the NUMA region
     * of the producer. Producer of different NUMA regions should not share
     * a single queue.
     * @param task Task to be scheduled.
     * @param numa_node_id NUMA region of the producer.
     */
    void push_back_remote(TaskInterface *task, const std::uint8_t numa_node_id) noexcept
    {
        _remote_queues[task->priority()][numa_node_id].push_back(task);
    }

    /**
     * Schedules a task to the local queue, which is not thread-safe. Only
     * the channel owner should spawn tasks this way.
     * @param task Task to be scheduled.
     */
    void push_back_local(TaskInterface *task) noexcept { _local_queues[task->priority()].push_back(task); }

    /**
     * Fill the task buffer with tasks from the backend queues.
     * @return Size of the buffer after filling it.
     */
    std::uint16_t fill() noexcept
    {
        // Fill with normal prioritized.
        auto size = fill<priority::normal>(_task_buffer.available_slots());

        // Fill with low prioritized.
        if (this->_task_buffer.empty())
        {
            size = fill<priority::low>(config::task_buffer_size());
        }

        return size;
    }

    /**
     * Fills the task buffer with tasks scheduled with a given priority.
     *
     * @tparam P Priority of the tasks.
     * @return Size of the task buffer after filling.
     */
    template <priority P> std::uint16_t fill() noexcept { return fill<P>(_task_buffer.available_slots()); }

    /**
     * @return Number of tasks available in the buffer and ready for execution.
     */
    [[nodiscard]] std::uint16_t size() const noexcept { return _task_buffer.size(); }

    /**
     * @return True, when the task buffer is empty. Backend queues may be have tasks.
     */
    [[nodiscard]] bool empty() const noexcept { return _task_buffer.empty(); }

    /**
     * Adds usage prediction of a resource to this channel.
     * @param usage Predicted usage.
     */
    void predict_usage(const resource::hint::expected_access_frequency usage) noexcept { _occupancy.predict(usage); }

    /**
     * Updates the usage prediction of this channel.
     * @param old_prediction So far predicted usage.
     * @param new_prediction New predicted usage.
     */
    void modify_predicted_usage(const resource::hint::expected_access_frequency old_prediction,
                                const resource::hint::expected_access_frequency new_prediction) noexcept
    {
        _occupancy.revoke(old_prediction);
        _occupancy.predict(new_prediction);
    }

    /**
     * @return Aggregated predicted usage.
     */
    [[nodiscard]] resource::hint::expected_access_frequency predicted_usage() const noexcept
    {
        return static_cast<resource::hint::expected_access_frequency>(_occupancy);
    }

    /**
     * @return True, whenever min. one prediction was "excessive".
     */
    [[nodiscard]] bool has_excessive_usage_prediction() const noexcept
    {
        return _occupancy.has_excessive_usage_prediction();
    }

private:
    // Backend queues for multiple produces in different NUMA regions and different priorities,
    alignas(64)
        std::array<std::array<util::MPSCQueue<TaskInterface>, memory::config::max_numa_nodes()>, 2> _remote_queues{};

    // Backend queues for a single producer (owning worker thread) and different priorities.
    alignas(64) std::array<util::Queue<TaskInterface>, 2> _local_queues{};

    // Buffer for ready-to-execute tasks.
    alignas(64) TaskBuffer<config::task_buffer_size()> _task_buffer;

    // Id of this channel.
    const std::uint16_t _id;

    // NUMA id of the worker thread owning this channel.
    const std::uint8_t _numa_node_id;

    // Holder of resource predictions of this channel.
    alignas(64) ChannelOccupancy _occupancy{};

    /**
     * Fills the task buffer with tasks scheduled with a given priority.
     *
     * @tparam P Priority.
     * @param available Number of maximal tasks to fill the task buffer.
     * @return Size of the task buffer after filling.
     */
    template <priority P> std::uint16_t fill(std::uint16_t available) noexcept
    {
        // 1) Fill up from the local queue.
        available -= _task_buffer.fill(_local_queues[P], available);

        if (available > 0U)
        {
            // 2) Fill up from remote queues; start with the NUMA-local one.
            for (auto i = 0U; i < _remote_queues[P].max_size(); ++i)
            {
                const auto numa_node_id = (_numa_node_id + i) & (_remote_queues[P].max_size() - 1U);
                available -= _task_buffer.fill(_remote_queues[P][numa_node_id], available);
            }
        }

        return _task_buffer.max_size() - available;
    }
};
} // namespace mx::tasking