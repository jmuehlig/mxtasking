#pragma once

#include <algorithm>
#include <cstdint>
#include <numa.h>
#include <sched.h>
#include <thread>

namespace mx::system {
/**
 * Encapsulates methods for retrieving information
 * about the hardware landscape.
 */
class topology
{
public:
    /**
     * @return Core where the caller is running.
     */
    static std::uint16_t core_id() { return std::uint16_t(sched_getcpu()); }

    /**
     * Reads the NUMA region identifier of the given core.
     *
     * @param core_id Id of the core.
     * @return Id of the NUMA region the core stays in.
     */
    static std::uint8_t node_id(const std::uint16_t core_id) { return std::max(numa_node_of_cpu(core_id), 0); }

    /**
     * @return The greatest NUMA region identifier.
     */
    static std::uint8_t max_node_id() { return std::uint8_t(numa_max_node()); }

    /**
     * @return Number of available cores.
     */
    static std::uint16_t count_cores() { return std::uint16_t(std::thread::hardware_concurrency()); }
};
} // namespace mx::system