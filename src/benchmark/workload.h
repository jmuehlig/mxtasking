#pragma once

#include "phase.h"
#include "workload_set.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <utility>

namespace benchmark {
class Workload
{
    friend std::ostream &operator<<(std::ostream &stream, const Workload &workload);

public:
    Workload() noexcept = default;
    ~Workload() noexcept = default;

    [[maybe_unused]] void build(const std::string &fill_workload_file, const std::string &mixed_workload_file)
    {
        _workload_set.build(fill_workload_file, mixed_workload_file);
    }

    [[maybe_unused]] void build(const std::uint64_t fill_inserts, const std::uint64_t mixed_inserts,
                                const std::uint64_t mixed_lookups, const std::uint64_t mixed_updates,
                                const std::uint64_t mixed_deletes)
    {
        _workload_set.build(fill_inserts, mixed_inserts, mixed_lookups, mixed_updates, mixed_deletes);
    }

    [[maybe_unused]] void shuffle() { _workload_set.shuffle(); }

    std::pair<std::uint64_t, std::uint64_t> next(std::uint64_t count) noexcept;

    [[nodiscard]] std::uint64_t size() const noexcept { return _workload_set[_current_phase].size(); }
    [[nodiscard]] bool empty() const noexcept { return _workload_set[_current_phase].empty(); }
    [[nodiscard]] bool empty(const phase phase) const noexcept { return _workload_set[phase].empty(); }

    void reset(const phase phase) noexcept
    {
        _current_phase = phase;
        _current_index = 0;
    }

    const NumericTuple &operator[](const std::size_t index) const noexcept
    {
        return _workload_set[_current_phase][index];
    }
    bool operator==(const phase phase) const noexcept { return _current_phase == phase; }
    explicit operator phase() const noexcept { return _current_phase; }

private:
    NumericWorkloadSet _workload_set;
    phase _current_phase = phase::FILL;

    alignas(64) std::atomic_uint64_t _current_index{0U};
};
} // namespace benchmark