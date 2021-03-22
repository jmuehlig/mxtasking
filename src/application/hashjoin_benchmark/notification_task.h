#pragma once
#include "listener.h"
#include <cstdint>
#include <cstdlib>
#include <mx/tasking/task.h>

namespace application::hash_join {
template <class N> class NotificationTask final : public mx::tasking::TaskInterface
{
public:
    NotificationTask(Listener<N> &listener) : _listener(listener) {}

    ~NotificationTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t /*core_id*/, const std::uint16_t channel_id) override
    {
        if (--_listener.pending_local(channel_id) == 0U)
        {
            if (--_listener.pending_global() == 0U)
            {
                _listener.notificator()(channel_id);
            }
        }

        return mx::tasking::TaskResult::make_remove();
    }

private:
    Listener<N> &_listener;
};
} // namespace application::hash_join