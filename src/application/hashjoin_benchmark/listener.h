#pragma once
#include "merge_task.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <mx/util/aligned_t.h>
#include <mx/util/core_set.h>
#include <mx/util/reference_counter.h>
#include <vector>

namespace application::hash_join {
template <class N> class Listener
{
public:
    Listener(const std::uint16_t count_cores, N &notificator) : _count_cores(count_cores), _notificator(notificator)
    {
        _pending_local_notifications.fill(mx::util::aligned_t<std::uint32_t>{0U});
        std::fill_n(_pending_local_notifications.begin(), count_cores, mx::util::aligned_t<std::uint32_t>{count_cores});

        _pending_global_notifications.store(count_cores);
    }

    ~Listener() = default;

    std::uint16_t count_cores() const noexcept { return _count_cores; }
    N &notificator() noexcept { return _notificator; }
    std::uint32_t &pending_local(const std::uint16_t channel_id) noexcept
    {
        return _pending_local_notifications[channel_id].value();
    }
    std::atomic_uint32_t &pending_global() noexcept { return _pending_global_notifications; }

private:
    const std::uint16_t _count_cores;
    N &_notificator;
    std::array<mx::util::aligned_t<std::uint32_t>, mx::tasking::config::max_cores()> _pending_local_notifications{};
    alignas(64) std::atomic_uint32_t _pending_global_notifications{0U};
};
} // namespace application::hash_join