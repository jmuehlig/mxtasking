#pragma once
#include "build_task.h"
#include "listener.h"
#include "notification_task.h"
#include "notifier.h"
#include "probe_task.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <mx/util/core_set.h>
#include <vector>

namespace application::hash_join {

template <class T> struct notifier_type
{
    using value = BuildFinishedNotifier;
};

template <> struct notifier_type<ProbeTask>
{
    using value = ProbeFinishedNotifier;
};

template <typename T> class PartitionTask final : public mx::tasking::TaskInterface
{
public:
    constexpr PartitionTask(Listener<typename notifier_type<T>::value> &listener, const std::uint32_t batch_size,
                            const std::size_t count, const mx::resource::ptr *hash_tables) noexcept
        : _listener(listener), _batch_size(batch_size), _count(count), _hash_tables(hash_tables)
    {
    }

    ~PartitionTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t channel_id) override
    {
        const auto count_cores = _listener.count_cores();

        auto build_probe_tasks = std::array<T *, mx::tasking::config::max_cores()>{nullptr};
        for (auto target_channel_id = 0U; target_channel_id < count_cores; ++target_channel_id)
        {
            if constexpr (std::is_same<T, BuildTask>::value)
            {
                build_probe_tasks[target_channel_id] = mx::tasking::runtime::new_task<T>(
                    core_id, _batch_size, mx::tasking::runtime::numa_node_id(target_channel_id));
            }
            else
            {
                build_probe_tasks[target_channel_id] = mx::tasking::runtime::new_task<T>(
                    core_id, _listener.notificator().result_set(target_channel_id), _batch_size,
                    mx::tasking::runtime::numa_node_id(target_channel_id));
            }

            build_probe_tasks[target_channel_id]->annotate(_hash_tables[target_channel_id], 64U);
        }

        auto *data = this->annotated_resource().template get<std::uint32_t>();
        const auto offset = channel_id * _count;
        for (auto data_index = 0U; data_index < _count; ++data_index)
        {
            const auto key = data[data_index];

            // Distribute key to core
            const auto target_channel_id = PartitionTask::hash(key) % count_cores;
            build_probe_tasks[target_channel_id]->emplace_back(offset + data_index, key);

            // Run specific task and create new.
            if (build_probe_tasks[target_channel_id]->size() == _batch_size)
            {
                mx::tasking::runtime::spawn(*build_probe_tasks[target_channel_id], channel_id);

                if constexpr (std::is_same<T, BuildTask>::value)
                {
                    build_probe_tasks[target_channel_id] = mx::tasking::runtime::new_task<T>(
                        core_id, _batch_size, mx::tasking::runtime::numa_node_id(target_channel_id));
                }
                else
                {
                    build_probe_tasks[target_channel_id] = mx::tasking::runtime::new_task<T>(
                        core_id, _listener.notificator().result_set(target_channel_id), _batch_size,
                        mx::tasking::runtime::numa_node_id(target_channel_id));
                }

                build_probe_tasks[target_channel_id]->annotate(_hash_tables[target_channel_id], 64U);
            }
        }

        for (auto target_channel_id = 0U; target_channel_id < count_cores; ++target_channel_id)
        {
            // Run last build/probe tasks that are not "full".
            mx::tasking::runtime::spawn(*build_probe_tasks[target_channel_id], channel_id);

            // Run notification tasks for every core, indicating that all
            // build/probe tasks of this core are dispatched.
            auto *notification_task =
                mx::tasking::runtime::new_task<NotificationTask<typename notifier_type<T>::value>>(core_id, _listener);
            notification_task->annotate(std::uint16_t(target_channel_id));
            mx::tasking::runtime::spawn(*notification_task, channel_id);
        }

        return mx::tasking::TaskResult::make_remove();
    }

private:
    Listener<typename notifier_type<T>::value> &_listener;
    const std::uint32_t _batch_size;
    const std::size_t _count;
    const mx::resource::ptr *_hash_tables;

    static std::uint16_t hash(const std::uint32_t key) { return std::hash<std::uint32_t>()(key); }
};
} // namespace application::hash_join