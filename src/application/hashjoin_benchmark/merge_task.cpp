#include "merge_task.h"
#include "benchmark.h"

using namespace application::hash_join;

MergeTask::MergeTask(const mx::util::core_set &cores, Benchmark *benchmark, const std::uint64_t output_per_core)
    : _benchmark(benchmark), _count_cores(cores.size())
{
    this->_result_sets = new mx::util::aligned_t<mx::util::vector<std::pair<std::size_t, std::size_t>>>[cores.size()];

    for (auto channel_id = 0U; channel_id < cores.size(); ++channel_id)
    {
        this->_result_sets[channel_id].value().reserve(cores.numa_node_id(channel_id), output_per_core);
    }
}

MergeTask::~MergeTask()
{
    delete[] this->_result_sets;
}

mx::tasking::TaskResult MergeTask::execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/)
{
    for (auto channel = 0U; channel < _count_cores; ++channel)
    {
        _count_output_tuples += result_set(channel).size();
    }

    _benchmark->stop();
    return mx::tasking::TaskResult::make_null();
}
