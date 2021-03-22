#pragma once
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace mx::system {
/**
 * Encapsulates methods for thread access.
 */
class thread
{
public:
    /**
     * Pins a thread to a given core.
     *
     * @param thread Thread to pin.
     * @param core_id Core where the thread should be pinned.
     * @return True, when pinning was successful.
     */
    static bool pin(std::thread &thread, const std::uint16_t core_id)
    {
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(core_id, &cpu_set);

        if (pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpu_set) != 0)
        {
            std::cerr << "Can not pin thread!" << std::endl;
            return false;
        }

        return true;
    }
};
} // namespace mx::system