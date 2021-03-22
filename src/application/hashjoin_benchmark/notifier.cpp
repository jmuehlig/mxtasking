#include "notifier.h"
#include <mx/tasking/runtime.h>

using namespace application::hash_join;

void BuildFinishedNotifier::operator()(const std::uint16_t channel_id)
{
    for (auto target_channel_id = 0U; target_channel_id < this->_count_cores; ++target_channel_id)
    {
        mx::tasking::runtime::spawn(*this->_probe_tasks[target_channel_id], channel_id);
    }
}

void ProbeFinishedNotifier::operator()(const std::uint16_t channel_id)
{
    mx::tasking::runtime::spawn(*this->_merge_task, channel_id);
}