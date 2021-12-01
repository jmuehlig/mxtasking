#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <mx/memory/config.h>
#include <mx/system/topology.h>
#include <mx/tasking/config.h>
#include <ostream>

namespace mx::util {
/**
 * The core set is used to identify cores included into MxTasking runtime.
 * Naively, we would just specify the number of used cores; this would reach
 * the limit when reordering the core ids (e.g. to include all cores of a
 * single NUMA region).
 */
class core_set
{
    friend std::ostream &operator<<(std::ostream &stream, const core_set &ore_set);

public:
    enum Order
    {
        Ascending,
        NUMAAware
    };

    constexpr core_set() noexcept : _core_identifier({0U}), _numa_nodes(0U) {}
    explicit core_set(std::initializer_list<std::uint16_t> &&core_ids) noexcept : core_set()
    {
        for (const auto core_id : core_ids)
        {
            emplace_back(core_id);
        }
    }
    ~core_set() noexcept = default;

    core_set &operator=(const core_set &other) noexcept = default;

    /**
     * Add a core to the core set.
     * @param core_identifier Logical identifier of the core.
     */
    void emplace_back(const std::uint16_t core_identifier) noexcept
    {
        _core_identifier[_size++] = core_identifier;
        _numa_nodes[system::topology::node_id(core_identifier)] = true;
    }

    std::uint16_t operator[](const std::uint16_t index) const noexcept { return _core_identifier[index]; }
    std::uint16_t front() const { return _core_identifier.front(); }
    std::uint16_t back() const { return _core_identifier.back(); }

    explicit operator bool() const noexcept { return _size > 0U; }

    /**
     * @return Number of included cores.
     */
    [[nodiscard]] std::uint16_t size() const noexcept { return _size; }

    /**
     * @return Number of included NUMA regions.
     */
    [[nodiscard]] std::uint16_t numa_nodes() const noexcept { return _numa_nodes.count(); }

    /**
     * NUMA node id of the given channel.
     * @param index Channel.
     * @return NUMA node id.
     */
    [[nodiscard]] std::uint8_t numa_node_id(const std::uint16_t index) const noexcept
    {
        return system::topology::node_id(_core_identifier[index]);
    }

    /**
     * @return Highest included core identifier.
     */
    [[nodiscard]] std::uint16_t max_core_id() const noexcept
    {
        return *std::max_element(_core_identifier.cbegin(), _core_identifier.cbegin() + _size);
    }

    /**
     * Builds the core set for a fixed number of cores and specified ordering.
     * @param cores Number of cores.
     * @param order The order can be "Ascending" (for using the systems order) or "NUMA Aware".
     * @return
     */
    static core_set build(std::uint16_t cores, Order order = Ascending);

    bool operator==(const core_set &other) const noexcept
    {
        return _core_identifier == other._core_identifier && _size == other._size && _numa_nodes == other._numa_nodes;
    }

    bool operator!=(const core_set &other) const noexcept
    {
        return _core_identifier != other._core_identifier || _size != other._size || _numa_nodes != other._numa_nodes;
    }

    /**
     * @param numa_node_id NUMA node identifier.
     * @return True, when the NUMA region is represented in the core set.
     */
    [[nodiscard]] bool has_core_of_numa_node(const std::uint8_t numa_node_id) const noexcept
    {
        return _numa_nodes.test(numa_node_id);
    }

    [[nodiscard]] auto begin() const noexcept { return _core_identifier.begin(); }
    [[nodiscard]] auto end() const noexcept { return _core_identifier.begin() + _size; }

private:
    // List of core identifiers.
    std::array<std::uint16_t, tasking::config::max_cores()> _core_identifier;

    // Number of cores in the set.
    std::uint16_t _size{0U};

    // Bitvector for represented NUMA regions.
    std::bitset<memory::config::max_numa_nodes()> _numa_nodes{0U};
};
} // namespace mx::util