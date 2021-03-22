#pragma once
#include <chrono>

namespace mx::memory {
class config
{
public:
    /**
     * @return Number of maximal provided NUMA regions.
     */
    static constexpr auto max_numa_nodes() { return 2U; }

    /**
     * Decreases the use of memory of external NUMA regions within the allocator.
     * @return True, when memory usage of external NUMA regions should be less.
     */
    static constexpr auto low_priority_for_external_numa() { return false; }

    /**
     * @return Interval of each epoch, if memory reclamation is used.
     */
    static constexpr auto epoch_interval() { return std::chrono::milliseconds(50U); }

    /**
     * @return True, if garbage is removed local.
     */
    static constexpr auto local_garbage_collection() { return false; }
};
} // namespace mx::memory