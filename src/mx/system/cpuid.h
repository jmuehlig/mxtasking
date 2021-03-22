#pragma once

#include <cstdint>

namespace mx::system {
/**
 * Encapsulates methods for checking features
 * of the system by calling cpuid instruction.
 */
class cpuid
{
public:
    /**
     * @return True, when restricted transactional memory
     *  is enabled.
     */
    static bool is_rtm_provided()
    {
        std::uint32_t eax = 0x7;
        std::uint32_t ebx;
        std::uint32_t ecx = 0x0;
        asm volatile("cpuid" : "=b"(ebx) : "a"(eax), "c"(ecx));

        return ebx & 0b100000000000;
    }
};
} // namespace mx::system