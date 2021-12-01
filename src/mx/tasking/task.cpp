#include "task.h"
#include "runtime.h"
#include <mx/system/topology.h>

using namespace mx::tasking;

TaskResult TaskResult::make_stop() noexcept
{
    auto *stop_task = runtime::new_task<StopTaskingTask>(system::topology::core_id());
    stop_task->annotate(std::uint16_t{0U});
    return TaskResult::make_succeed_and_remove(stop_task);
}

TaskResult StopTaskingTask::execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/)
{
    runtime::stop();
    return TaskResult::make_remove();
}