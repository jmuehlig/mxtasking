#pragma once
#include "task.h"
#include <mx/system/cache.h>
#include <utility>

namespace mx::tasking {
/**
 * A prefetch slot is part of the prefetch buffer used for task
 * and resource prefetching
 * A slot can contain up to one task and one resource that are
 * prefetched by the channel.
 */
class PrefetchSlot
{
public:
    constexpr PrefetchSlot() noexcept = default;
    ~PrefetchSlot() = default;

    PrefetchSlot &operator=(TaskInterface *task) noexcept
    {
        _task = task;
        if (task->has_resource_annotated())
        {
            _resource = std::make_pair(task->annotated_resource().get(), task->annotated_resource_size());
        }
        return *this;
    }

    void operator()() noexcept
    {
        if (_task != nullptr)
        {
            system::cache::prefetch<system::cache::L1, system::cache::write>(_task);
            _task = nullptr;
        }

        if (std::get<0>(_resource) != nullptr)
        {
            system::cache::prefetch_range<system::cache::LLC, system::cache::read>(std::get<0>(_resource),
                                                                                   std::get<1>(_resource));
            std::get<0>(_resource) = nullptr;
        }
    }

private:
    void *_task = nullptr;
    std::pair<void *, std::uint16_t> _resource = std::make_pair(nullptr, 0U);
};
} // namespace mx::tasking