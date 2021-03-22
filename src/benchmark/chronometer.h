#pragma once

#include "perf.h"
#include "phase.h"
#include <chrono>
#include <json.hpp>
#include <mx/tasking/config.h>
#include <mx/tasking/profiling/statistic.h>
#include <mx/tasking/runtime.h>
#include <mx/util/core_set.h>
#include <numeric>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace benchmark {
/**
 * The InterimResult is part of the chronometer, which in turn holds
 * all results during a benchmark.
 */
template <typename P> class InterimResult
{
    friend std::ostream &operator<<(std::ostream &stream, const InterimResult &result)
    {
        stream << result.core_count() << "\t" << result.iteration() << "\t" << result.phase() << "\t"
               << result.time().count() << " ms"
               << "\t" << result.throughput() << " op/s";

        for (const auto &[name, value] : result.performance_counter())
        {
            const auto value_per_operation = value / double(result.operation_count());
            stream << "\t" << value_per_operation << " " << name << "/op";
        }

        if constexpr (mx::tasking::config::task_statistics())
        {
            stream << "\t" << result.executed_writer_tasks() / double(result.operation_count()) << " writer/op";
            stream << "\t" << result.executed_reader_tasks() / double(result.operation_count()) << " reader/op";
            stream << "\t" << result.scheduled_tasks_on_core() / double(result.operation_count()) << " on-channel/op";
            stream << "\t" << result.scheduled_tasks_off_core() / double(result.operation_count()) << " off-channel/op";
            stream << "\t" << result.worker_fills() / double(result.operation_count()) << " fills/op";
        }

        return stream << std::flush;
    }

public:
    InterimResult(const std::uint64_t operation_count, const P &phase, const std::uint16_t iteration,
                  const std::uint16_t core_count, const std::chrono::milliseconds time,
                  std::vector<PerfCounter> &counter, std::unordered_map<std::uint16_t, std::uint64_t> executed_tasks,
                  std::unordered_map<std::uint16_t, std::uint64_t> executed_reader_tasks,
                  std::unordered_map<std::uint16_t, std::uint64_t> executed_writer_tasks,
                  std::unordered_map<std::uint16_t, std::uint64_t> scheduled_tasks,
                  std::unordered_map<std::uint16_t, std::uint64_t> scheduled_tasks_on_core,
                  std::unordered_map<std::uint16_t, std::uint64_t> scheduled_tasks_off_core,
                  std::unordered_map<std::uint16_t, std::uint64_t> worker_fills)
        : _operation_count(operation_count), _phase(phase), _iteration(iteration), _core_count(core_count), _time(time),
          _executed_tasks(std::move(executed_tasks)), _executed_reader_tasks(std::move(executed_reader_tasks)),
          _executed_writer_tasks(std::move(executed_writer_tasks)), _scheduled_tasks(std::move(scheduled_tasks)),
          _scheduled_tasks_on_core(std::move(scheduled_tasks_on_core)),
          _scheduled_tasks_off_core(std::move(scheduled_tasks_off_core)), _worker_fills(std::move(worker_fills))
    {
        for (auto &c : counter)
        {
            _performance_counter.emplace_back(std::make_pair(c.name(), c.read()));
        }
    }

    ~InterimResult() = default;

    std::uint64_t operation_count() const noexcept { return _operation_count; }
    const P &phase() const noexcept { return _phase; }
    std::uint16_t iteration() const noexcept { return _iteration; }
    std::uint16_t core_count() const noexcept { return _core_count; }
    std::chrono::milliseconds time() const noexcept { return _time; }
    double throughput() const { return _operation_count / (_time.count() / 1000.0); }
    const std::vector<std::pair<std::string, double>> &performance_counter() const noexcept
    {
        return _performance_counter;
    }

    [[maybe_unused]] std::uint64_t executed_tasks() const noexcept { return sum(_executed_tasks); }
    [[maybe_unused]] std::uint64_t executed_reader_tasks() const noexcept { return sum(_executed_reader_tasks); }
    [[maybe_unused]] std::uint64_t executed_writer_tasks() const noexcept { return sum(_executed_writer_tasks); }
    [[maybe_unused]] std::uint64_t scheduled_tasks() const noexcept { return sum(_scheduled_tasks); }
    [[maybe_unused]] std::uint64_t scheduled_tasks_on_core() const noexcept { return sum(_scheduled_tasks_on_core); }
    [[maybe_unused]] std::uint64_t scheduled_tasks_off_core() const noexcept { return sum(_scheduled_tasks_off_core); }
    [[maybe_unused]] std::uint64_t worker_fills() const noexcept { return sum(_worker_fills); }

    std::uint64_t executed_tasks(const std::uint16_t channel_id) const noexcept
    {
        return _executed_tasks.at(channel_id);
    }
    std::uint64_t executed_reader_tasks(const std::uint16_t channel_id) const noexcept
    {
        return _executed_reader_tasks.at(channel_id);
    }
    std::uint64_t executed_writer_tasks(const std::uint16_t channel_id) const noexcept
    {
        return _executed_writer_tasks.at(channel_id);
    }
    std::uint64_t scheduled_tasks(const std::uint16_t channel_id) const noexcept
    {
        return _scheduled_tasks.at(channel_id);
    }
    std::uint64_t scheduled_tasks_on_core(const std::uint16_t channel_id) const noexcept
    {
        return _scheduled_tasks_on_core.at(channel_id);
    }
    std::uint64_t scheduled_tasks_off_core(const std::uint16_t channel_id) const noexcept
    {
        return _scheduled_tasks_off_core.at(channel_id);
    }
    std::uint64_t worker_fills(const std::uint16_t channel_id) const noexcept { return _worker_fills.at(channel_id); }

    [[nodiscard]] nlohmann::json to_json() const noexcept
    {
        auto json = nlohmann::json{};
        json["iteration"] = iteration();
        json["cores"] = core_count();
        json["phase"] = phase();
        json["throughput"] = throughput();
        for (const auto &[name, value] : performance_counter())
        {
            json[name] = value / double(operation_count());
        }

        if constexpr (mx::tasking::config::task_statistics())
        {
            json["executed-writer-tasks"] = executed_writer_tasks() / double(operation_count());
            json["executed-reader-tasks"] = executed_reader_tasks() / double(operation_count());
            json["scheduled-tasks-on-channel"] = scheduled_tasks_on_core() / double(operation_count());
            json["scheduled-tasks-off-channel"] = scheduled_tasks_off_core() / double(operation_count());
            json["buffer-fills"] = worker_fills() / double(operation_count());
        }

        return json;
    }

private:
    const std::uint64_t _operation_count;
    const P &_phase;
    const std::uint16_t _iteration;
    const std::uint16_t _core_count;
    const std::chrono::milliseconds _time;
    std::vector<std::pair<std::string, double>> _performance_counter;
    const std::unordered_map<std::uint16_t, std::uint64_t> _executed_tasks;
    const std::unordered_map<std::uint16_t, std::uint64_t> _executed_reader_tasks;
    const std::unordered_map<std::uint16_t, std::uint64_t> _executed_writer_tasks;
    const std::unordered_map<std::uint16_t, std::uint64_t> _scheduled_tasks;
    const std::unordered_map<std::uint16_t, std::uint64_t> _scheduled_tasks_on_core;
    const std::unordered_map<std::uint16_t, std::uint64_t> _scheduled_tasks_off_core;
    const std::unordered_map<std::uint16_t, std::uint64_t> _worker_fills;

    std::uint64_t sum(const std::unordered_map<std::uint16_t, std::uint64_t> &map) const noexcept
    {
        return std::accumulate(map.begin(), map.end(), 0U,
                               [](const auto &current, const auto &item) { return current + item.second; });
    }
};
/**
 * The Chronometer is the "benchmark clock", which will be started and stopped
 * before and after each benchmark run. On stopping, the chronometer will calculate
 * used time, persist performance counter values, and mx::tasking statistics.
 */
template <typename P> class Chronometer
{
public:
    Chronometer() = default;
    ~Chronometer() = default;

    void start(const P phase, const std::uint16_t iteration, const mx::util::core_set &core_set)
    {
        _current_phase = phase;
        _current_iteration = iteration;
        _core_set = core_set;

        _perf.start();
        _start = std::chrono::steady_clock::now();
    }

    InterimResult<P> stop(const std::uint64_t count_operations)
    {
        const auto end = std::chrono::steady_clock::now();
        _perf.stop();

        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - _start);

        return {count_operations,
                _current_phase,
                _current_iteration,
                _core_set.size(),
                milliseconds,
                _perf.counter(),
                statistic_map(mx::tasking::profiling::Statistic::Executed),
                statistic_map(mx::tasking::profiling::Statistic::ExecutedReader),
                statistic_map(mx::tasking::profiling::Statistic::ExecutedWriter),
                statistic_map(mx::tasking::profiling::Statistic::Scheduled),
                statistic_map(mx::tasking::profiling::Statistic::ScheduledOnChannel),
                statistic_map(mx::tasking::profiling::Statistic::ScheduledOffChannel),
                statistic_map(mx::tasking::profiling::Statistic::Fill)};
    }

    void add(PerfCounter &performance_counter) { _perf.add(performance_counter); }

private:
    std::uint16_t _current_iteration{0U};
    P _current_phase;
    mx::util::core_set _core_set;
    alignas(64) Perf _perf;
    alignas(64) std::chrono::steady_clock::time_point _start;

    std::unordered_map<std::uint16_t, std::uint64_t> statistic_map(
        const mx::tasking::profiling::Statistic::Counter counter)
    {
        std::unordered_map<std::uint16_t, std::uint64_t> statistics;
        for (auto i = 0U; i < mx::tasking::runtime::channels(); ++i)
        {
            statistics[i] = mx::tasking::runtime::statistic(counter, i);
        }
        return statistics;
    }
};
} // namespace benchmark