#include "benchmark.h"
#include "build_task.h"
#include "inline_hashtable.h"
#include "partition_task.h"
#include "tpch_table_reader.h"
#include <mx/memory/global_heap.h>
#include <mx/tasking/runtime.h>

using namespace application::hash_join;

Benchmark::Benchmark(
    benchmark::Cores &&cores, const std::uint16_t iterations, std::vector<std::uint32_t> &&batches,
    std::tuple<std::pair<std::string, std::uint16_t>, std::pair<std::string, std::uint16_t>> &&join_table_files,
    const bool use_performance_counter, std::string &&result_file_name)
    : _cores(std::move(cores)), _iterations(iterations), _batches(std::move(batches)),
      _result_file_name(std::move(result_file_name))
{
    if (use_performance_counter)
    {
        this->_chronometer.add(benchmark::Perf::CYCLES);
        this->_chronometer.add(benchmark::Perf::INSTRUCTIONS);
        this->_chronometer.add(benchmark::Perf::STALLS_MEM_ANY);
        this->_chronometer.add(benchmark::Perf::SW_PREFETCH_ACCESS_NTA);
        this->_chronometer.add(benchmark::Perf::SW_PREFETCH_ACCESS_WRITE);
    }

    std::cout << "core configuration: \n" << this->_cores.dump(2) << std::endl;

    std::vector<std::uint32_t> left_keys;
    const auto &left_table = std::get<0>(std::get<0>(join_table_files));
    const auto left_column_index = std::get<1>(std::get<0>(join_table_files));
    application::hash_join::TPCHTableReader::read(
        left_table, [&left_keys, left_column_index](const std::uint16_t index, const std::string &value) {
            if (index == left_column_index)
            {
                left_keys.emplace_back(std::stoul(value));
            }
        });

    std::vector<std::uint32_t> right_keys;
    const auto &right_table = std::get<0>(std::get<1>(join_table_files));
    const auto right_column_index = std::get<1>(std::get<1>(join_table_files));
    application::hash_join::TPCHTableReader::read(
        right_table, [&right_keys, right_column_index](const std::uint16_t index, const std::string &value) {
            if (index == right_column_index)
            {
                right_keys.emplace_back(std::stoul(value));
            }
        });

    this->_join_keys = std::make_tuple(std::move(left_keys), std::move(right_keys));

    std::cout << "workload: " << left_table << "." << left_column_index << " (#" << std::get<0>(this->_join_keys).size()
              << ")"
              << " JOIN " << right_table << "." << right_column_index << " (#" << std::get<1>(this->_join_keys).size()
              << ")"
              << "\n"
              << std::endl;
}

void Benchmark::start()
{
    const auto count_cores = this->_cores.current().size();
    const auto count_left_keys = std::get<0>(this->_join_keys).size();
    const auto count_left_keys_per_core = Benchmark::tuples_per_core(count_left_keys, count_cores);
    const auto count_right_keys = std::get<1>(this->_join_keys).size();
    const auto count_right_keys_per_core = Benchmark::tuples_per_core(count_right_keys, count_cores);

    this->_merge_task = std::make_unique<MergeTask>(this->_cores.current(), this, count_right_keys_per_core);

    // Clear notifications.
    this->_build_notification = BuildFinishedNotifier{count_cores};
    this->_probe_notification = ProbeFinishedNotifier{this->_merge_task.get()};
    this->_build_listener = std::make_unique<Listener<BuildFinishedNotifier>>(count_cores, this->_build_notification);
    this->_probe_listener = std::make_unique<Listener<ProbeFinishedNotifier>>(count_cores, this->_probe_notification);

    // Build hash_tables.
    this->_hash_tables.reset(new mx::resource::ptr[count_cores]); // NOLINT

    for (auto channel_id = 0U; channel_id < count_cores; ++channel_id)
    {
        const auto needed_keys = std::size_t(count_left_keys_per_core * 1.5);
        const auto needed_bytes = InlineHashtable<std::uint32_t, std::size_t>::needed_bytes(needed_keys);
        this->_hash_tables.get()[channel_id] =
            mx::tasking::runtime::new_resource<InlineHashtable<std::uint32_t, std::size_t>>(
                needed_bytes,
                mx::resource::hint{std::uint16_t(channel_id), mx::synchronization::isolation_level::Exclusive,
                                   mx::synchronization::protocol::Queue},
                needed_bytes);
    }

    /// Dispatch left table
    auto partition_build_tasks = std::array<mx::tasking::TaskInterface *, mx::tasking::config::max_cores()>{nullptr};

    for (auto i = 0U; i < count_cores; ++i)
    {
        const auto count_left_keys_for_core = i < count_cores - 1U
                                                  ? count_left_keys_per_core
                                                  : (count_left_keys - (count_cores - 1U) * count_left_keys_per_core);
        const auto count_right_keys_for_core =
            i < count_cores - 1U ? count_right_keys_per_core
                                 : (count_right_keys - (count_cores - 1U) * count_right_keys_per_core);

        // Build chunk for local dispatching
        auto left_chunk = mx::tasking::runtime::to_resource(
            &std::get<0>(this->_join_keys)[i * count_left_keys_per_core],
            mx::resource::hint{std::uint16_t(i), mx::synchronization::isolation_level::Exclusive,
                               mx::synchronization::protocol::Queue});
        auto right_chunk = mx::tasking::runtime::to_resource(
            &std::get<1>(this->_join_keys)[i * count_right_keys_per_core],
            mx::resource::hint{std::uint16_t(i), mx::synchronization::isolation_level::Exclusive,
                               mx::synchronization::protocol::Queue});

        // Run dispatcher task.
        auto *partition_probe_task = mx::tasking::runtime::new_task<PartitionTask<ProbeTask>>(
            0U, *this->_probe_listener, this->_batches[this->_current_batch_index], count_right_keys_for_core,
            this->_hash_tables.get());
        partition_probe_task->annotate(right_chunk, 64U);
        this->_build_notification.dispatch_probe_task(i, partition_probe_task);

        auto *partition_build_task = mx::tasking::runtime::new_task<PartitionTask<BuildTask>>(
            0U, *this->_build_listener, this->_batches[this->_current_batch_index], count_left_keys_for_core,
            this->_hash_tables.get());
        partition_build_task->annotate(left_chunk, 64U);
        partition_build_tasks[i] = partition_build_task;
    }

    // Here we go
    this->_chronometer.start(this->_batches[this->_current_batch_index], this->_current_iteration,
                             this->_cores.current());
    for (auto i = 0U; i < count_cores; ++i)
    {
        mx::tasking::runtime::spawn(*(partition_build_tasks[i]), 0U);
    }
}

void Benchmark::stop()
{
    // Stop and print time (and performance counter).
    const auto result = this->_chronometer.stop(this->_merge_task->count_tuples());
    mx::tasking::runtime::stop();

    std::cout << result << std::endl;

    // Dump results to file.
    if (this->_result_file_name.empty() == false)
    {
        std::ofstream result_file_stream(this->_result_file_name, std::ofstream::app);
        result_file_stream << result.to_json().dump() << std::endl;
    }
}

const mx::util::core_set &Benchmark::core_set()
{
    if (this->_current_iteration == std::numeric_limits<std::uint16_t>::max())
    {
        // This is the very first time we start the benchmark.
        this->_current_iteration = 0U;
        return this->_cores.next();
    }

    for (auto i = 0U; i < this->_cores.current().size(); ++i)
    {
        mx::tasking::runtime::delete_resource<InlineHashtable<std::uint32_t, std::size_t>>(this->_hash_tables.get()[i]);
    }

    // Run the next iteration.
    if (++this->_current_iteration < this->_iterations)
    {
        return this->_cores.current();
    }
    this->_current_iteration = 0U;

    if (++this->_current_batch_index < this->_batches.size())
    {
        return this->_cores.current();
    }
    this->_current_batch_index = 0U;

    // At this point, all phases and all iterations for the current core configuration
    // are done. Increase the cores.
    return this->_cores.next();
}

std::uint64_t Benchmark::tuples_per_core(const std::uint64_t count_join_keys, const std::uint16_t count_cores) noexcept
{
    const auto cache_lines = (count_join_keys * sizeof(std::uint32_t)) / 64U;
    const auto cache_lines_per_core = cache_lines / count_cores;
    auto p = 1U;
    while (p < cache_lines_per_core)
    {
        p += 64U;
    }

    return p * (64U / sizeof(std::uint32_t));
}
