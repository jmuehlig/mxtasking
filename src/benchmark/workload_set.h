#pragma once

#include "phase.h"
#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace benchmark {
class NumericTuple
{
public:
    enum Type
    {
        INSERT,
        LOOKUP,
        UPDATE,
        DELETE
    };

    constexpr NumericTuple(const Type type, const std::uint64_t key) : _type(type), _key(key) {}
    constexpr NumericTuple(const Type type, const std::uint64_t key, const std::int64_t value)
        : _type(type), _key(key), _value(value)
    {
    }

    NumericTuple(NumericTuple &&) noexcept = default;
    NumericTuple(const NumericTuple &) = default;

    ~NumericTuple() = default;

    NumericTuple &operator=(NumericTuple &&) noexcept = default;

    [[nodiscard]] std::uint64_t key() const { return _key; };
    [[nodiscard]] std::int64_t value() const { return _value; }

    bool operator==(const Type type) const { return _type == type; }

private:
    Type _type;
    std::uint64_t _key;
    std::int64_t _value = 0;
};

class NumericWorkloadSet
{
    friend std::ostream &operator<<(std::ostream &stream, const NumericWorkloadSet &workload_set);

public:
    NumericWorkloadSet() = default;
    ~NumericWorkloadSet() = default;

    void build(const std::string &fill_workload_file, const std::string &mixed_workload_file);
    void build(std::uint64_t fill_inserts, std::uint64_t mixed_inserts, std::uint64_t mixed_lookups,
               std::uint64_t mixed_updates, std::uint64_t mixed_deletes);
    void shuffle();

    [[nodiscard]] const std::vector<NumericTuple> &fill() const noexcept { return _data_sets[0]; }
    [[nodiscard]] const std::vector<NumericTuple> &mixed() const noexcept { return _data_sets[1]; }
    const std::vector<NumericTuple> &operator[](const phase phase) const noexcept
    {
        return _data_sets[static_cast<std::uint16_t>(phase)];
    }

    explicit operator bool() const { return fill().empty() == false || mixed().empty() == false; }

private:
    std::array<std::vector<NumericTuple>, 2> _data_sets;
    bool _mixed_phase_contains_update = false;

    static std::ostream &nice_print(std::ostream &stream, std::size_t number) noexcept;
};
} // namespace benchmark