#pragma once
#include <bitset>
#include <cstdint>

namespace mx::tasking {
/**
 * Persists the channel load for the last 64 requests.
 */
class Load
{
public:
    constexpr Load() = default;
    ~Load() = default;

    Load &operator+=(const bool hit) noexcept
    {
        _hits <<= 1;
        _hits.set(0, hit);
        return *this;
    }

    Load &operator|=(const Load &other) noexcept
    {
        _hits |= other._hits;
        return *this;
    }

    /**
     * @return Number of successful requests.
     */
    [[nodiscard]] std::size_t count() const noexcept { return _hits.count(); }

    bool operator<(const Load &other) const noexcept { return _hits.count() < other._hits.count(); }
    bool operator<(const std::size_t other) const noexcept { return _hits.count() < other; }

private:
    // Bitvector of the last 64 requests.
    std::bitset<64> _hits{0U};
};
} // namespace mx::tasking