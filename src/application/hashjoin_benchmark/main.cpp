#include "benchmark.h"
#include <argparse.hpp>
#include <iostream>
#include <mx/system/environment.h>
#include <utility>
#include <vector>

using namespace application::hash_join;

std::pair<Benchmark *, std::uint16_t> create_benchmark(int count_arguments, char **arguments);

int main(int count_arguments, char **arguments)
{
    auto [benchmark, prefetch_distance] = create_benchmark(count_arguments, arguments);

    if (mx::system::Environment::is_numa_balancing_enabled())
    {
        std::cout << "[Warn] NUMA balancing may be enabled, set '/proc/sys/kernel/numa_balancing' to '0'" << std::endl;
    }

    if (benchmark == nullptr)
    {
        return 1;
    }

    mx::util::core_set cores{};

    while ((cores = benchmark->core_set()))
    {
        mx::tasking::runtime_guard _(false, cores, prefetch_distance);
        benchmark->start();
    }

    delete benchmark;

    return 0;
}

std::pair<Benchmark *, std::uint16_t> create_benchmark(int count_arguments, char **arguments)
{
    argparse::ArgumentParser argument_parser("hashjoin_benchmark");
    argument_parser.add_argument("cores")
        .help("Range of the number of cores (1 for using 1 core, 1: for using 1 up to available cores, 1:4 for using "
              "cores from 1 to 4).")
        .default_value(std::string("1"));
    argument_parser.add_argument("-s", "--steps")
        .help("Steps, how number of cores is increased (1,2,4,6,.. for -s 2).")
        .default_value(std::uint16_t(2U))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-i", "--iterations")
        .help("Number of iterations for each workload")
        .default_value(std::uint16_t(1U))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-sco", "--system-core-order")
        .help("Use systems core order. If not, cores are ordered by node id (should be preferred).")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-p", "--perf")
        .help("Use performance counter.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-pd", "--prefetch-distance")
        .help("Distance of prefetched data objects (0 = disable prefetching).")
        .default_value(std::uint16_t(0U))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-o", "--out")
        .help("Name of the file, the results will be written to.")
        .default_value(std::string(""));
    argument_parser.add_argument("--batch")
        .help("Number of tuples build/probed together; comma separated as string (e.g. \"64,128,256\")")
        .default_value(std::string("128"));
    argument_parser.add_argument("-R").help("Data file of left relation.").default_value(std::string("customer.tbl"));
    argument_parser.add_argument("-R-key")
        .help("Index of join key of R")
        .default_value(std::uint16_t(0U))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("-S").help("Data file of right relation.").default_value(std::string("orders.tbl"));
    argument_parser.add_argument("-S-key")
        .help("Index of join key of S")
        .default_value(std::uint16_t(1U))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });

    // Parse arguments.
    try
    {
        argument_parser.parse_args(count_arguments, arguments);
    }
    catch (std::runtime_error &e)
    {
        std::cout << argument_parser << std::endl;
        return std::make_pair(nullptr, 0U);
    }

    auto order =
        argument_parser.get<bool>("-sco") ? mx::util::core_set::Order::Ascending : mx::util::core_set::Order::NUMAAware;
    auto cores =
        benchmark::Cores({argument_parser.get<std::string>("cores"), argument_parser.get<std::uint16_t>("-s"), order});

    std::vector<std::uint32_t> build_probe_batches;
    auto batches = std::stringstream{argument_parser.get<std::string>("--batch")};
    std::string batch;
    while (std::getline(batches, batch, ','))
    {
        build_probe_batches.emplace_back(std::stoul(batch));
    }

    // Join relations
    auto r = std::make_pair(argument_parser.get<std::string>("-R"), argument_parser.get<std::uint16_t>("-R-key"));
    auto s = std::make_pair(argument_parser.get<std::string>("-S"), argument_parser.get<std::uint16_t>("-S-key"));

    // Create the benchmark.
    auto *benchmark = new Benchmark(std::move(cores), argument_parser.get<std::uint16_t>("-i"),
                                    std::move(build_probe_batches), std::make_tuple(std::move(r), std::move(s)),
                                    argument_parser.get<bool>("-p"), argument_parser.get<std::string>("-o"));

    return {benchmark, argument_parser.get<std::uint16_t>("-pd")};
}