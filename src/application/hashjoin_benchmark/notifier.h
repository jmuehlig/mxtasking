#pragma once

#include "merge_task.h"
#include <array>
#include <mx/tasking/task.h>
#include <mx/util/vector.h>

namespace application::hash_join {

class BuildFinishedNotifier
{
public:
    constexpr BuildFinishedNotifier() = default;
    constexpr BuildFinishedNotifier(const std::uint16_t count_cores) : _count_cores(count_cores)
    {
        _probe_tasks.fill(nullptr);
    }

    BuildFinishedNotifier &operator=(BuildFinishedNotifier &&) = default;

    ~BuildFinishedNotifier() = default;

    void dispatch_probe_task(const std::uint16_t index, mx::tasking::TaskInterface *task) noexcept
    {
        _probe_tasks[index] = task;
    }

    void operator()(std::uint16_t channel_id);

private:
    std::uint16_t _count_cores{0U};
    std::array<mx::tasking::TaskInterface *, mx::tasking::config::max_cores()> _probe_tasks{};
};

class ProbeFinishedNotifier
{
public:
    constexpr ProbeFinishedNotifier() = default;
    constexpr ProbeFinishedNotifier(MergeTask *merge_task) : _merge_task(merge_task) {}

    ProbeFinishedNotifier &operator=(ProbeFinishedNotifier &&) = default;

    ~ProbeFinishedNotifier() = default;

    void operator()(std::uint16_t channel_id);

    mx::util::vector<std::pair<std::size_t, std::size_t>> &result_set(const std::uint16_t channel_id) noexcept
    {
        return _merge_task->result_set(channel_id);
    }

private:
    MergeTask *_merge_task{nullptr};
};

} // namespace application::hash_join