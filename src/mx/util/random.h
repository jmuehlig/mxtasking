#pragma once

#include <array>
#include <cstdint>

namespace mx::util {
/**
 * Random generator for cheap pseudo random numbers.
 */
class alignas(64) Random
{
public:
    Random() noexcept;
    explicit Random(std::uint32_t seed) noexcept;

    /**
     * @return Next pseudo random number.
     */
    std::int32_t next() noexcept;

    /**
     * @param max_value Max value.
     * @return Next pseudo random number in range (0,max_value].
     */
    std::uint32_t next(const std::uint64_t max_value) noexcept { return next() % max_value; }

private:
    std::array<std::uint32_t, 7> _register;
    std::uint32_t _multiplier = 0;
    std::uint32_t _ic_state = 0;
    const std::uint32_t _addend = 0;
};
} // namespace mx::util
