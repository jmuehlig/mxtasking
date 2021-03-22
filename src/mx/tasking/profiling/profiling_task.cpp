#include "profiling_task.h"
#include <fstream>
#include <json.hpp>
#include <mx/memory/global_heap.h>
#include <mx/tasking/runtime.h>

using namespace mx::tasking::profiling;

ProfilingTask::ProfilingTask(mx::util::maybe_atomic<bool> &is_running, mx::tasking::Channel &channel)
    : _is_running(is_running), _channel(channel)
{
    _idle_ranges.reserve(1 << 16);
}

mx::tasking::TaskResult ProfilingTask::execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/)
{
    IdleRange range;

    while (this->_is_running && this->_channel.empty())
    {
        this->_channel.fill();
    }

    range.stop();

    if (range.nanoseconds() > 10U)
    {
        this->_idle_ranges.emplace_back(std::move(range));
    }

    if (this->_is_running)
    {
        return tasking::TaskResult::make_succeed(this);
    }

    return tasking::TaskResult::make_null();
}

Profiler::~Profiler()
{
    for (auto *task : this->_tasks)
    {
        delete task;
    }
}

void Profiler::profile(const std::string &profiling_output_file)
{
    for (auto *task : this->_tasks)
    {
        delete task;
    }
    this->_tasks.clear();

    this->_profiling_output_file.emplace(profiling_output_file);
    this->_start = std::chrono::steady_clock::now();
}

void Profiler::profile(util::maybe_atomic<bool> &is_running, Channel &channel)
{
    auto *task =
        new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(ProfilingTask))) ProfilingTask(is_running, channel);
    task->annotate(channel.id());
    task->annotate(mx::tasking::priority::low);
    this->_tasks.push_back(task);
    mx::tasking::runtime::spawn(*task);
}

void Profiler::stop()
{
    const auto end = std::chrono::steady_clock::now();
    const auto end_relative_nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - this->_start).count();
    if (this->_profiling_output_file.has_value())
    {
        auto output = nlohmann::json{};
        for (auto *task : this->_tasks)
        {
            if (task != nullptr && task->idle_ranges().empty() == false)
            {
                nlohmann::json channel_output;
                channel_output["channel"] = task->annotated_channel();
                nlohmann::json ranges{};
                for (const auto &range : task->idle_ranges())
                {
                    const auto normalized = range.normalize(this->_start);
                    auto normalized_json = nlohmann::json{};
                    normalized_json["s"] = std::get<0>(normalized);
                    normalized_json["e"] = std::get<1>(normalized);
                    ranges.push_back(std::move(normalized_json));
                }

                channel_output["ranges"] = std::move(ranges);
                output.push_back(std::move(channel_output));
            }
        }

        nlohmann::json end_output;
        end_output["end"] = end_relative_nanoseconds;
        output.push_back(std::move(end_output));

        std::ofstream out_file{this->_profiling_output_file.value()};
        out_file << output.dump() << std::endl;
    }

    this->_profiling_output_file = std::nullopt;
}