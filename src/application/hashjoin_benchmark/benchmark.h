#pragma once

#include "listener.h"
#include "merge_task.h"
#include "notifier.h"
#include <benchmark/chronometer.h>
#include <benchmark/cores.h>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

namespace application::hash_join {

class Benchmark
{
public:
    Benchmark(
        benchmark::Cores &&cores, std::uint16_t iterations, std::vector<std::uint32_t> &&batches,
        std::tuple<std::pair<std::string, std::uint16_t>, std::pair<std::string, std::uint16_t>> &&join_table_files,
        bool use_performance_counter, std::string &&result_file_name);

    ~Benchmark() = default;

    /**
     * @return Core set the benchmark should run in the current iteration.
     */
    const mx::util::core_set &core_set();

    void start();

    void stop();

private:
    // Collection of cores the benchmark should run on.
    benchmark::Cores _cores;

    // Number of iterations the benchmark should use.
    const std::uint16_t _iterations;

    // Current iteration within the actual core set.
    std::uint16_t _current_iteration = std::numeric_limits<std::uint16_t>::max();

    // Number of tuples that are probed/build together.
    const std::vector<std::uint32_t> _batches;
    std::uint16_t _current_batch_index{0U};

    // Name of the file to print results to.
    const std::string _result_file_name;

    // Keys to join.
    std::tuple<std::vector<std::uint32_t>, std::vector<std::uint32_t>> _join_keys;

    std::unique_ptr<mx::resource::ptr> _hash_tables;

    std::unique_ptr<Listener<BuildFinishedNotifier>> _build_listener;
    std::unique_ptr<Listener<ProbeFinishedNotifier>> _probe_listener;

    std::unique_ptr<MergeTask> _merge_task;

    alignas(64) BuildFinishedNotifier _build_notification;
    ProbeFinishedNotifier _probe_notification;

    // Chronometer for starting/stopping time and performance counter.
    alignas(64) benchmark::Chronometer<std::uint32_t> _chronometer;

    static std::uint64_t tuples_per_core(std::uint64_t count_join_keys, std::uint16_t count_cores) noexcept;
};
} // namespace application::hash_join