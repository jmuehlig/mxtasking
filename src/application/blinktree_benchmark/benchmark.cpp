#include "benchmark.h"
#include <cstdlib>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <mx/memory/global_heap.h>

using namespace application::blinktree_benchmark;

Benchmark::Benchmark(benchmark::Cores &&cores, const std::uint16_t iterations, std::string &&fill_workload_file,
                     std::string &&mixed_workload_file, const bool use_performance_counter,
                     const mx::synchronization::isolation_level node_isolation_level,
                     const mx::synchronization::protocol preferred_synchronization_method,
                     const bool print_tree_statistics, const bool check_tree, std::string &&result_file_name,
                     std::string &&statistic_file_name, std::string &&tree_file_name, const bool profile)
    : _cores(std::move(cores)), _iterations(iterations), _node_isolation_level(node_isolation_level),
      _preferred_synchronization_method(preferred_synchronization_method),
      _print_tree_statistics(print_tree_statistics), _check_tree(check_tree),
      _result_file_name(std::move(result_file_name)), _statistic_file_name(std::move(statistic_file_name)),
      _tree_file_name(std::move(tree_file_name)), _profile(profile)
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

    this->_workload.build(fill_workload_file, mixed_workload_file);
    if (this->_workload.empty(benchmark::phase::FILL) && this->_workload.empty(benchmark::phase::MIXED))
    {
        std::exit(1);
    }

    std::cout << "workload: " << this->_workload << "\n" << std::endl;
}

void Benchmark::start()
{
    // Reset tree.
    if (this->_tree == nullptr)
    {
        this->_tree = std::make_unique<db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>>(
            this->_node_isolation_level, this->_preferred_synchronization_method);
    }

    // Reset request scheduler.
    if (this->_request_scheduler.empty() == false)
    {
        this->_request_scheduler.clear();
    }

    // Create one request scheduler per core.
    for (auto core_index = 0U; core_index < this->_cores.current().size(); core_index++)
    {
        const auto channel_id = core_index;
        auto *request_scheduler = mx::tasking::runtime::new_task<RequestSchedulerTask>(
            0U, core_index, channel_id, this->_workload, this->_cores.current(), this->_tree.get(), this);
        mx::tasking::runtime::spawn(*request_scheduler, 0U);
        this->_request_scheduler.push_back(request_scheduler);
    }
    this->_open_requests = this->_request_scheduler.size();

    // Start measurement.
    if (this->_profile)
    {
        mx::tasking::runtime::profile(this->profile_file_name());
    }
    this->_chronometer.start(static_cast<std::uint16_t>(static_cast<benchmark::phase>(this->_workload)),
                             this->_current_iteration + 1, this->_cores.current());
}

const mx::util::core_set &Benchmark::core_set()
{
    if (this->_current_iteration == std::numeric_limits<std::uint16_t>::max())
    {
        // This is the very first time we start the benchmark.
        this->_current_iteration = 0U;
        return this->_cores.next();
    }

    // Switch from fill to mixed phase.
    if (this->_workload == benchmark::phase::FILL && this->_workload.empty(benchmark::phase::MIXED) == false)
    {
        this->_workload.reset(benchmark::phase::MIXED);
        return this->_cores.current();
    }
    this->_workload.reset(benchmark::phase::FILL);

    // Run the next iteration.
    if (++this->_current_iteration < this->_iterations)
    {
        return this->_cores.current();
    }
    this->_current_iteration = 0U;

    // At this point, all phases and all iterations for the current core configuration
    // are done. Increase the cores.
    return this->_cores.next();
}

void Benchmark::requests_finished()
{
    const auto open_requests = --this->_open_requests;

    if (open_requests == 0U) // All request schedulers are done.
    {
        // Stop and print time (and performance counter).
        const auto result = this->_chronometer.stop(this->_workload.size());
        mx::tasking::runtime::stop();
        std::cout << result << std::endl;

        // Dump results to file.
        if (this->_result_file_name.empty() == false)
        {
            std::ofstream result_file_stream(this->_result_file_name, std::ofstream::app);
            result_file_stream << result.to_json().dump() << std::endl;
        }

        // Dump statistics to file.
        if constexpr (mx::tasking::config::task_statistics())
        {
            if (this->_statistic_file_name.empty() == false)
            {
                std::ofstream statistic_file_stream(this->_statistic_file_name, std::ofstream::app);
                nlohmann::json statistic_json;
                statistic_json["iteration"] = result.iteration();
                statistic_json["cores"] = result.core_count();
                statistic_json["phase"] = result.phase();
                statistic_json["scheduled"] = nlohmann::json();
                statistic_json["scheduled-on-channel"] = nlohmann::json();
                statistic_json["scheduled-off-channel"] = nlohmann::json();
                statistic_json["executed"] = nlohmann::json();
                statistic_json["executed-reader"] = nlohmann::json();
                statistic_json["executed-writer"] = nlohmann::json();
                statistic_json["buffer-fills"] = nlohmann::json();
                for (auto i = 0U; i < this->_cores.current().size(); i++)
                {
                    const auto core_id = std::int32_t{this->_cores.current()[i]};
                    const auto core_id_string = std::to_string(core_id);
                    statistic_json["scheduled"][core_id_string] =
                        result.scheduled_tasks(core_id) / double(result.operation_count());
                    statistic_json["scheduled-on-core"][core_id_string] =
                        result.scheduled_tasks_on_core(core_id) / double(result.operation_count());
                    statistic_json["scheduled-off-core"][core_id_string] =
                        result.scheduled_tasks_off_core(core_id) / double(result.operation_count());
                    statistic_json["executed"][core_id_string] =
                        result.executed_tasks(core_id) / double(result.operation_count());
                    statistic_json["executed-reader"][core_id_string] =
                        result.executed_reader_tasks(core_id) / double(result.operation_count());
                    statistic_json["executed-writer"][core_id_string] =
                        result.executed_writer_tasks(core_id) / double(result.operation_count());
                    statistic_json["fill"][core_id_string] =
                        result.worker_fills(core_id) / double(result.operation_count());
                }

                statistic_file_stream << statistic_json.dump(2) << std::endl;
            }
        }

        // Check and print the tree.
        if (this->_check_tree)
        {
            this->_tree->check();
        }

        if (this->_print_tree_statistics)
        {
            this->_tree->print_statistics();
        }

        const auto is_last_phase =
            this->_workload == benchmark::phase::MIXED || this->_workload.empty(benchmark::phase::MIXED);

        // Dump the tree.
        if (this->_tree_file_name.empty() == false && is_last_phase)
        {
            std::ofstream tree_file_stream(this->_tree_file_name);
            tree_file_stream << static_cast<nlohmann::json>(*(this->_tree)).dump() << std::endl;
        }

        // Delete the tree to free the hole memory.
        if (is_last_phase)
        {
            this->_tree.reset(nullptr);
        }
    }
}

std::string Benchmark::profile_file_name() const
{
    return "profiling-" + std::to_string(this->_cores.current().size()) + "-cores" + "-phase-" +
           std::to_string(static_cast<std::uint16_t>(static_cast<benchmark::phase>(this->_workload))) + "-iteration-" +
           std::to_string(this->_current_iteration) + ".json";
}