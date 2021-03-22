#include "benchmark.h"
#include <argparse.hpp>
#include <benchmark/cores.h>
#include <mx/system/environment.h>
#include <mx/system/thread.h>
#include <mx/tasking/runtime.h>
#include <mx/util/core_set.h>
#include <tuple>

using namespace application::blinktree_benchmark;

/**
 * Instantiates the BLink-Tree benchmark with CLI arguments.
 * @param count_arguments Number of CLI arguments.
 * @param arguments Arguments itself.
 *
 * @return Instance of the benchmark and parameters for tasking runtime.
 */
std::tuple<Benchmark *, std::uint16_t, bool> create_benchmark(int count_arguments, char **arguments);

/**
 * Starts the benchmark.
 *
 * @param count_arguments Number of CLI arguments.
 * @param arguments Arguments itself.
 *
 * @return Return code of the application.
 */
int main(int count_arguments, char **arguments)
{
    if (mx::system::Environment::is_numa_balancing_enabled())
    {
        std::cout << "[Warn] NUMA balancing may be enabled, set '/proc/sys/kernel/numa_balancing' to '0'" << std::endl;
    }

    auto [benchmark, prefetch_distance, use_system_allocator] = create_benchmark(count_arguments, arguments);
    if (benchmark == nullptr)
    {
        return 1;
    }

    mx::util::core_set cores{};

    while ((cores = benchmark->core_set()))
    {
        mx::tasking::runtime_guard _(use_system_allocator, cores, prefetch_distance);
        benchmark->start();
    }

    delete benchmark;

    return 0;
}

std::tuple<Benchmark *, std::uint16_t, bool> create_benchmark(int count_arguments, char **arguments)
{
    // Set up arguments.
    argparse::ArgumentParser argument_parser("blinktree_benchmark");
    argument_parser.add_argument("cores")
        .help("Range of the number of cores (1 for using 1 core, 1: for using 1 up to available cores, 1:4 for using "
              "cores from 1 to 4).")
        .default_value(std::string("1"));
    /* Not used for the moment.
    argument_parser.add_argument("-c", "--channels-per-core")
        .help("Number of how many channels used per core.")
        .default_value(std::uint16_t(1))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    */
    argument_parser.add_argument("-s", "--steps")
        .help("Steps, how number of cores is increased (1,2,4,6,.. for -s 2).")
        .default_value(std::uint16_t(2))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-i", "--iterations")
        .help("Number of iterations for each workload")
        .default_value(std::uint16_t(1))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-sco", "--system-core-order")
        .help("Use systems core order. If not, cores are ordered by node id (should be preferred).")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-p", "--perf")
        .help("Use performance counter.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--exclusive")
        .help("Are all node accesses exclusive?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--latched")
        .help("Prefer latch for synchronization?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--olfit")
        .help("Prefer OLFIT for synchronization?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--sync4me")
        .help("Let the tasking layer decide the synchronization primitive.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--print-stats")
        .help("Print tree statistics after every iteration.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--disable-check")
        .help("Disable tree check while benchmarking.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-f", "--workload-files")
        .help("Files containing the workloads (workloads/fill workloads/mixed for example).")
        .nargs(2)
        .default_value(
            std::vector<std::string>{"workloads/fill_randint_workloada", "workloads/mixed_randint_workloada"});
    argument_parser.add_argument("-pd", "--prefetch-distance")
        .help("Distance of prefetched data objects (0 = disable prefetching).")
        .default_value(std::uint16_t(0))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("--system-allocator")
        .help("Use the systems malloc interface to allocate tasks (default disabled).")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-ot", "--out-tree")
        .help("Name of the file, the tree will be written in json format.")
        .default_value(std::string(""));
    argument_parser.add_argument("-os", "--out-statistics")
        .help("Name of the file, the task statistics will be written in json format.")
        .default_value(std::string(""));
    argument_parser.add_argument("-o", "--out")
        .help("Name of the file, the results will be written to.")
        .default_value(std::string(""));
    argument_parser.add_argument("--profiling")
        .help("Enable profiling (default disabled).")
        .implicit_value(true)
        .default_value(false);

    // Parse arguments.
    try
    {
        argument_parser.parse_args(count_arguments, arguments);
    }
    catch (std::runtime_error &e)
    {
        std::cout << argument_parser << std::endl;
        return {nullptr, 0U, false};
    }

    auto order =
        argument_parser.get<bool>("-sco") ? mx::util::core_set::Order::Ascending : mx::util::core_set::Order::NUMAAware;
    auto cores =
        benchmark::Cores({argument_parser.get<std::string>("cores"), argument_parser.get<std::uint16_t>("-s"), order});
    auto workload_files = argument_parser.get<std::vector<std::string>>("-f");
    const auto isolation_level = argument_parser.get<bool>("--exclusive")
                                     ? mx::synchronization::isolation_level::Exclusive
                                     : mx::synchronization::isolation_level::ExclusiveWriter;
    auto preferred_synchronization_method = mx::synchronization::protocol::Queue;
    if (argument_parser.get<bool>("--latched"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::Latch;
    }
    else if (argument_parser.get<bool>("--olfit"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::OLFIT;
    }
    else if (argument_parser.get<bool>("--sync4me"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::None;
    }

    // Create the benchmark.
    auto *benchmark =
        new Benchmark(std::move(cores), argument_parser.get<std::uint16_t>("-i"), std::move(workload_files[0]),
                      std::move(workload_files[1]), argument_parser.get<bool>("-p"), isolation_level,
                      preferred_synchronization_method, argument_parser.get<bool>("--print-stats"),
                      argument_parser.get<bool>("--disable-check") == false, argument_parser.get<std::string>("-o"),
                      argument_parser.get<std::string>("-os"), argument_parser.get<std::string>("-ot"),
                      argument_parser.get<bool>("--profiling"));

    return {benchmark, argument_parser.get<std::uint16_t>("-pd"), argument_parser.get<bool>("--system-allocator")};
}