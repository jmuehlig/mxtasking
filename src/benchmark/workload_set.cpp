#include "workload_set.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

using namespace benchmark;

void NumericWorkloadSet::build(const std::string &fill_workload_file, const std::string &mixed_workload_file)
{
    auto parse = [](auto &file_stream, std::vector<NumericTuple> &data_set) -> bool {
        std::srand(1337);
        std::string op_name;
        std::uint64_t key{};

        bool contains_update = false;

        while (file_stream >> op_name >> key)
        {
            if (op_name == "INSERT")
            {
                contains_update = true;
                data_set.emplace_back(NumericTuple{NumericTuple::INSERT, key, std::rand()});
            }
            else if (op_name == "READ")
            {
                data_set.emplace_back(NumericTuple{NumericTuple::LOOKUP, key});
            }
            else if (op_name == "UPDATE")
            {
                contains_update = true;
                data_set.emplace_back(NumericTuple{NumericTuple::UPDATE, key, std::rand()});
            }
        }

        return contains_update;
    };

    std::mutex out_mutex;
    std::thread fill_thread{[this, &out_mutex, &parse, &fill_workload_file]() {
        std::ifstream fill_file(fill_workload_file);
        if (fill_file.good())
        {
            parse(fill_file, this->_data_sets[static_cast<std::size_t>(phase::FILL)]);
        }
        else
        {
            std::lock_guard lock{out_mutex};
            std::cerr << "Could not open workload file '" << fill_workload_file << "'." << std::endl;
        }
    }};

    std::thread mixed_thread{[this, &out_mutex, &parse, &mixed_workload_file]() {
        std::ifstream mixed_file(mixed_workload_file);
        if (mixed_file.good())
        {
            this->_mixed_phase_contains_update =
                parse(mixed_file, this->_data_sets[static_cast<std::size_t>(phase::MIXED)]);
        }
        else
        {
            std::lock_guard lock{out_mutex};
            std::cerr << "Could not open workload file '" << mixed_workload_file << "'." << std::endl;
        }
    }};

    fill_thread.join();
    mixed_thread.join();
}

void NumericWorkloadSet::build(const std::uint64_t fill_inserts, const std::uint64_t mixed_inserts,
                               const std::uint64_t mixed_lookups, const std::uint64_t mixed_updates,
                               const std::uint64_t mixed_deletes)
{
    std::srand(1337);
    this->_data_sets[static_cast<std::uint8_t>(phase::FILL)].reserve(fill_inserts);
    this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].reserve(mixed_inserts + mixed_lookups + mixed_updates +
                                                                      mixed_deletes);

    for (auto i = 0U; i < fill_inserts; ++i)
    {
        this->_data_sets[static_cast<std::uint8_t>(phase::FILL)].emplace_back(
            NumericTuple{NumericTuple::INSERT, i + 1U, std::rand()});
    }

    this->_mixed_phase_contains_update = mixed_inserts > 0U || mixed_deletes > 0U || mixed_updates > 0U;

    for (auto i = fill_inserts; i < fill_inserts + mixed_inserts; ++i)
    {
        this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].emplace_back(
            NumericTuple{NumericTuple::INSERT, i + 1U, std::rand()});
    }

    for (auto i = 0U; i < mixed_lookups; ++i)
    {
        this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].push_back(
            {NumericTuple::LOOKUP, this->_data_sets[static_cast<std::uint16_t>(phase::FILL)][i % fill_inserts].key()});
    }

    for (auto i = 0U; i < mixed_updates; ++i)
    {
        this->_data_sets[static_cast<std::size_t>(phase::MIXED)].push_back(
            {NumericTuple::UPDATE, this->_data_sets[static_cast<std::uint16_t>(phase::FILL)][i % fill_inserts].key(),
             std::rand()});
    }

    for (auto i = 0U; i < mixed_deletes; ++i)
    {
        this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].push_back(
            {NumericTuple::DELETE, this->_data_sets[static_cast<std::uint16_t>(phase::FILL)][i % fill_inserts].key()});
    }
}

void NumericWorkloadSet::shuffle()
{
    std::srand(1337U + 42U);
    std::random_device random_device;
    std::mt19937 mersenne_twister_engine(random_device());

    std::shuffle(this->_data_sets[static_cast<std::uint8_t>(phase::FILL)].begin(),
                 this->_data_sets[static_cast<std::uint8_t>(phase::FILL)].end(), mersenne_twister_engine);
    std::shuffle(this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].begin(),
                 this->_data_sets[static_cast<std::uint8_t>(phase::MIXED)].end(), mersenne_twister_engine);
}

std::ostream &NumericWorkloadSet::nice_print(std::ostream &stream, const std::size_t number) noexcept
{
    if (number >= 1000000U)
    {
        return stream << (number / 1000000U) << "m";
    }

    if (number >= 1000U)
    {
        return stream << (number / 1000U) << "k";
    }

    return stream << number;
}

namespace benchmark {
std::ostream &operator<<(std::ostream &stream, const NumericWorkloadSet &workload)
{
    const auto has_fill_and_mixed = workload[phase::FILL].empty() == false && workload[phase::MIXED].empty() == false;

    if (workload[phase::FILL].empty() == false)
    {
        stream << "fill: ";
        NumericWorkloadSet::nice_print(stream, workload[phase::FILL].size());
    }

    if (has_fill_and_mixed)
    {
        stream << " / ";
    }

    if (workload[phase::MIXED].empty() == false)
    {
        stream << (workload._mixed_phase_contains_update ? "mixed: " : "read-only: ");
        NumericWorkloadSet::nice_print(stream, workload[phase::MIXED].size());
    }

    return stream << std::flush;
}
} // namespace benchmark
