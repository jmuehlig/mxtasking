#include "worker.h"
#include "config.h"
#include "runtime.h"
#include "task.h"
#include <cassert>
#include <mx/system/builtin.h>
#include <mx/system/topology.h>
#include <mx/util/random.h>

using namespace mx::tasking;

Worker::Worker(const std::uint16_t id, const std::uint16_t target_core_id, const std::uint16_t target_numa_node_id,
               const util::maybe_atomic<bool> &is_running, const std::uint16_t prefetch_distance,
               memory::reclamation::LocalEpoch &local_epoch,
               const std::atomic<memory::reclamation::epoch_t> &global_epoch, profiling::Statistic &statistic) noexcept
    : _target_core_id(target_core_id), _prefetch_distance(prefetch_distance),
      _channel(id, target_numa_node_id, prefetch_distance), _local_epoch(local_epoch), _global_epoch(global_epoch),
      _statistic(statistic), _is_running(is_running)
{
}

void Worker::execute()
{
    while (this->_is_running == false)
    {
        system::builtin::pause();
    }

    TaskInterface *task;
    const auto core_id = system::topology::core_id();
    assert(this->_target_core_id == core_id && "Worker not pinned to correct core.");
    const auto channel_id = this->_channel.id();

    while (this->_is_running)
    {
        if constexpr (config::memory_reclamation() == config::UpdateEpochPeriodically)
        {
            this->_local_epoch.enter(this->_global_epoch);
        }

        this->_channel_size = this->_channel.fill();

        if constexpr (config::task_statistics())
        {
            this->_statistic.increment<profiling::Statistic::Fill>(channel_id);
        }

        while ((task = this->_channel.next()) != nullptr)
        {
            // Whenever the worker-local task-buffer falls under
            // the prefetch distance, we re-fill the buffer to avoid
            // empty slots in the prefetch-buffer.
            if (--this->_channel_size <= this->_prefetch_distance)
            {
                if constexpr (config::memory_reclamation() == config::UpdateEpochPeriodically)
                {
                    this->_local_epoch.enter(this->_global_epoch);
                }

                this->_channel_size = this->_channel.fill();
                if constexpr (config::task_statistics())
                {
                    this->_statistic.increment<profiling::Statistic::Fill>(channel_id);
                }
            }

            if constexpr (config::task_statistics())
            {
                this->_statistic.increment<profiling::Statistic::Executed>(channel_id);
                if (task->has_resource_annotated())
                {
                    if (task->is_readonly())
                    {
                        this->_statistic.increment<profiling::Statistic::ExecutedReader>(channel_id);
                    }
                    else
                    {
                        this->_statistic.increment<profiling::Statistic::ExecutedWriter>(channel_id);
                    }
                }
            }

            // Based on the annotated resource and its synchronization
            // primitive, we choose the fitting execution context.
            auto result = TaskResult{};
            switch (Worker::synchronization_primitive(task))
            {
            case synchronization::primitive::ScheduleWriter:
                result = this->execute_optimistic(core_id, channel_id, task);
                break;
            case synchronization::primitive::OLFIT:
                result = this->execute_olfit(core_id, channel_id, task);
                break;
            case synchronization::primitive::ScheduleAll:
            case synchronization::primitive::None:
                result = task->execute(core_id, channel_id);
                break;
            case synchronization::primitive::ReaderWriterLatch:
                result = Worker::execute_reader_writer_latched(core_id, channel_id, task);
                break;
            case synchronization::primitive::ExclusiveLatch:
                result = Worker::execute_exclusive_latched(core_id, channel_id, task);
                break;
            }

            // The task-chain may be finished at time the
            // task has no successor. Otherwise, we spawn
            // the successor task.
            if (result.has_successor())
            {
                runtime::spawn(*static_cast<TaskInterface *>(result), channel_id);
            }

            if (result.is_remove())
            {
                runtime::delete_task(core_id, task);
            }
        }
    }
}

TaskResult Worker::execute_exclusive_latched(const std::uint16_t core_id, const std::uint16_t channel_id,
                                             mx::tasking::TaskInterface *const task)
{
    auto *resource = resource::ptr_cast<resource::ResourceInterface>(task->annotated_resource());

    resource::ResourceInterface::scoped_exclusive_latch _{resource};
    return task->execute(core_id, channel_id);
}

TaskResult Worker::execute_reader_writer_latched(const std::uint16_t core_id, const std::uint16_t channel_id,
                                                 mx::tasking::TaskInterface *const task)
{
    auto *resource = resource::ptr_cast<resource::ResourceInterface>(task->annotated_resource());

    // Reader do only need to acquire a "read-only" latch.
    if (task->is_readonly())
    {
        resource::ResourceInterface::scoped_rw_latch<false> _{resource};
        return task->execute(core_id, channel_id);
    }

    {
        resource::ResourceInterface::scoped_rw_latch<true> _{resource};
        return task->execute(core_id, channel_id);
    }
}

TaskResult Worker::execute_optimistic(const std::uint16_t core_id, const std::uint16_t channel_id,
                                      mx::tasking::TaskInterface *const task)
{
    auto *optimistic_resource = resource::ptr_cast<resource::ResourceInterface>(task->annotated_resource());

    if (task->is_readonly())
    {
        // For readers running at a different channel than writer,
        // we need to validate the version of the resource. This
        // comes along with saving the tasks state on a stack and
        // re-running the task, whenever the version check failed.
        if (task->annotated_resource().channel_id() != channel_id)
        {
            return this->execute_optimistic_read(core_id, channel_id, optimistic_resource, task);
        }

        // Whenever the task is executed at the same channel
        // where writing tasks are executed, we do not need to
        // synchronize because no write can happen.
        return task->execute(core_id, channel_id);
    }

    // Writers, however, need to acquire the version to tell readers, that
    // the resource is modified. This is done by making the version odd before
    // writing to the resource and even afterwards. Here, we can use a simple
    // fetch_add operation, because writers are serialized on the channel.
    {
        resource::ResourceInterface::scoped_optimistic_latch _{optimistic_resource};
        return task->execute(core_id, channel_id);
    }
}

TaskResult Worker::execute_olfit(const std::uint16_t core_id, const std::uint16_t channel_id, TaskInterface *const task)
{
    auto *optimistic_resource = resource::ptr_cast<resource::ResourceInterface>(task->annotated_resource());

    if (task->is_readonly())
    {
        return this->execute_optimistic_read(core_id, channel_id, optimistic_resource, task);
    }

    // Writers, however, need to acquire the version to tell readers, that
    // the resource is modified. This is done by making the version odd before
    // writing to the resource and even afterwards. Here, we need to use compare
    // xchg because writers can appear on every channel.
    {
        resource::ResourceInterface::scoped_olfit_latch _{optimistic_resource};
        return task->execute(core_id, channel_id);
    }
}

TaskResult Worker::execute_optimistic_read(const std::uint16_t core_id, const std::uint16_t channel_id,
                                           resource::ResourceInterface *optimistic_resource, TaskInterface *const task)
{
    if constexpr (config::memory_reclamation() == config::UpdateEpochOnRead)
    {
        this->_local_epoch.enter(this->_global_epoch);
    }

    // The current state of the task is saved for
    // restoring if the read operation failed, but
    // the task was maybe modified.
    this->_task_stack.save(task);

    do
    {
        const auto version = optimistic_resource->version();
        const auto result = task->execute(core_id, channel_id);

        if (optimistic_resource->is_version_valid(version))
        {
            if constexpr (config::memory_reclamation() == config::UpdateEpochOnRead)
            {
                this->_local_epoch.leave();
            }
            return result;
        }

        // At this point, the version check failed and we need
        // to re-run the read operation.
        this->_task_stack.restore(task);
    } while (true);
}