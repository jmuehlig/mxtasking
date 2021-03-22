#pragma once

#include <cstdint>
#include <mx/util/core_set.h>
#include <ostream>
#include <string>
#include <vector>

namespace benchmark {
/**
 * Set of core_sets used for a benchmark that should be performed over
 * different core counts to benchmark scalability.
 * Can be created from min and max cores (i.e. 1 core to 32 cores) or from
 * string identifying the cores (i.e. "1:32").
 */
class Cores
{
    friend std::ostream &operator<<(std::ostream &stream, const Cores &cores);

public:
    Cores(std::uint16_t min_cores, std::uint16_t max_cores, std::uint16_t steps, mx::util::core_set::Order order);
    Cores(const std::string &cores, std::uint16_t steps, mx::util::core_set::Order order);
    Cores(Cores &&) noexcept = default;

    ~Cores() = default;

    const mx::util::core_set &next()
    {
        const auto current_index = _current_index++;
        if (current_index < _core_sets.size())
        {
            return _core_sets[current_index];
        }

        return _empty_core_set;
    }

    [[nodiscard]] const mx::util::core_set &current() const noexcept { return _core_sets[_current_index - 1]; }
    [[nodiscard]] std::size_t size() const noexcept { return _core_sets.size(); }

    void reset() { _current_index = 0U; }

    [[nodiscard]] std::string dump(std::uint8_t indent) const;

private:
    std::vector<mx::util::core_set> _core_sets;
    std::uint16_t _current_index = 0U;
    const mx::util::core_set _empty_core_set;

    void add_for_range(std::uint16_t min_cores, std::uint16_t max_cores, std::uint16_t steps,
                       mx::util::core_set::Order order);
};
} // namespace benchmark