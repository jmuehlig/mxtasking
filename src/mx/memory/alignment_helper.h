#pragma once

#include <cstdint>

namespace mx::memory {
/**
 * Helper for setting the correct size on aligned allocation:
 *  The allocation size has to be a multiple of the alignment.
 */
class alignment_helper
{
public:
    template <typename T> static constexpr T next_multiple(const T value, const T base)
    {
        if (value > base)
        {
            const auto mod = value % base;
            if (mod == 0U)
            {
                return value;
            }

            return value + base - mod;
        }

        return base;
    }

    static constexpr bool is_power_of_two(const std::uint64_t value)
    {
        return ((value != 0U) && ((value & (value - 1U)) == 0U));
    }

    static constexpr std::uint64_t next_power_of_two(const std::uint64_t value)
    {
        return is_power_of_two(value) ? value : 1ULL << (sizeof(std::uint64_t) * 8 - __builtin_clzll(value));
    }
};
} // namespace mx::memory