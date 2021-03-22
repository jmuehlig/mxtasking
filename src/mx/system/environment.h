#pragma once

#include <fstream>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
public:
    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        std::ifstream numa_balancing_file("/proc/sys/kernel/numa_balancing");
        auto is_enabled = std::int32_t{};
        if (numa_balancing_file >> is_enabled)
        {
            return !(is_enabled == 0);
        }

        return true;
    }

    static constexpr auto is_sse2()
    {
#ifdef USE_SSE2
        return true;
#else
        return false;
#endif
    }
};
} // namespace mx::system