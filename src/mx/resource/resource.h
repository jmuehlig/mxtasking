#pragma once

#include "resource_interface.h"
#include <cassert>
#include <cstdint>
#include <mx/memory/alignment_helper.h>
#include <mx/memory/tagged_ptr.h>
#include <mx/synchronization/synchronization.h>
#include <mx/util/random.h>
#include <new>

namespace mx::resource {
/**
 * Hint for creating resources by the resource interface.
 * Encapsulates the requested numa region, synchronization requirements
 * and expected access frequency.
 */
class hint
{
public:
    enum expected_access_frequency : std::uint8_t
    {
        excessive = 0U,
        high = 1U,
        normal = 2U,
        unused = 3U,
    };

    enum expected_read_write_ratio : std::uint8_t
    {
        heavy_read = 0U,
        mostly_read = 1U,
        balanced = 2U,
        mostly_written = 3U,
        heavy_written = 4U
    };

    constexpr explicit hint(const std::uint8_t node_id) noexcept : _numa_node_id(node_id) {}
    constexpr explicit hint(const std::uint16_t channel_id) noexcept : _channel_id(channel_id) {}
    constexpr explicit hint(const synchronization::isolation_level isolation_level) noexcept
        : _isolation_level(isolation_level)
    {
    }
    constexpr explicit hint(const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency)
    {
    }
    constexpr hint(const std::uint16_t channel_id, const synchronization::isolation_level isolation_level) noexcept
        : _channel_id(channel_id), _isolation_level(isolation_level)
    {
    }
    constexpr hint(const std::uint8_t node_id, const synchronization::isolation_level isolation_level) noexcept
        : _numa_node_id(node_id), _isolation_level(isolation_level)
    {
    }
    constexpr hint(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                   const synchronization::protocol preferred_protocol) noexcept
        : _numa_node_id(node_id), _isolation_level(isolation_level), _preferred_protocol(preferred_protocol)
    {
    }

    constexpr hint(const std::uint16_t channel_id, const synchronization::isolation_level isolation_level,
                   const synchronization::protocol preferred_protocol) noexcept
        : _channel_id(channel_id), _isolation_level(isolation_level), _preferred_protocol(preferred_protocol)
    {
    }

    constexpr hint(const std::uint8_t node_id, const expected_access_frequency access_frequency) noexcept
        : _numa_node_id(node_id), _access_frequency(access_frequency)
    {
    }
    constexpr hint(const synchronization::isolation_level isolation_level,
                   const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency), _isolation_level(isolation_level)
    {
    }
    constexpr hint(const synchronization::isolation_level isolation_level,
                   const synchronization::protocol preferred_protocol,
                   const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }
    constexpr hint(const synchronization::isolation_level isolation_level,
                   const synchronization::protocol preferred_protocol, const expected_access_frequency access_frequency,
                   const expected_read_write_ratio read_write_ratio) noexcept
        : _access_frequency(access_frequency), _read_write_ratio(read_write_ratio), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }
    constexpr hint(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                   const expected_access_frequency access_frequency) noexcept
        : _numa_node_id(node_id), _access_frequency(access_frequency), _isolation_level(isolation_level)
    {
    }
    constexpr hint(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                   const synchronization::protocol preferred_protocol,
                   const expected_access_frequency access_frequency) noexcept
        : _numa_node_id(node_id), _access_frequency(access_frequency), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }

    constexpr hint(hint &&) noexcept = default;

    ~hint() = default;

    [[nodiscard]] bool has_numa_node_id() const noexcept
    {
        return _numa_node_id < std::numeric_limits<std::uint8_t>::max();
    }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return _numa_node_id; }

    [[nodiscard]] bool has_channel_id() const noexcept
    {
        return _channel_id < std::numeric_limits<std::uint16_t>::max();
    }
    [[nodiscard]] std::uint16_t channel_id() const noexcept { return _channel_id; }
    [[nodiscard]] expected_access_frequency access_frequency() const noexcept { return _access_frequency; }
    [[nodiscard]] expected_read_write_ratio read_write_ratio() const noexcept { return _read_write_ratio; }
    [[nodiscard]] synchronization::isolation_level isolation_level() const noexcept { return _isolation_level; }
    [[nodiscard]] synchronization::protocol preferred_protocol() const noexcept { return _preferred_protocol; }

    bool operator==(const synchronization::isolation_level isolation_level) const noexcept
    {
        return _isolation_level == isolation_level;
    }

    bool operator!=(const synchronization::isolation_level isolation_level) const noexcept
    {
        return _isolation_level != isolation_level;
    }

    bool operator==(const synchronization::protocol protocol) const noexcept { return _preferred_protocol == protocol; }

    bool operator!=(const synchronization::protocol protocol) const noexcept { return _preferred_protocol != protocol; }

private:
    hint() = default;

    // Preferred NUMA region; no preference by default.
    const std::uint8_t _numa_node_id{std::numeric_limits<std::uint8_t>::max()};

    // Preferred channel; no preference by default.
    const std::uint16_t _channel_id{std::numeric_limits<std::uint16_t>::max()};

    // Expected access frequency; normal by default.
    const enum expected_access_frequency _access_frequency { expected_access_frequency::normal };

    // Expected read/write ratio; normal by default.
    const expected_read_write_ratio _read_write_ratio{expected_read_write_ratio::balanced};

    // Preferred isolation level; no synchronization by default.
    const synchronization::isolation_level _isolation_level{synchronization::isolation_level::None};

    // Preferred synchronization protocol (queue, latch, ...); no synchronization by default.
    const synchronization::protocol _preferred_protocol{synchronization::protocol::None};
};

/**
 * Information of a resource, stored within
 * the pointer to the resource.
 */
class information
{
public:
    constexpr information() noexcept : _channel_id(0U), _synchronization_primitive(0U) {}
    explicit information(const std::uint16_t channel_id,
                         const synchronization::primitive synchronization_primitive) noexcept
        : _channel_id(channel_id), _synchronization_primitive(static_cast<std::uint16_t>(synchronization_primitive))
    {
    }

    ~information() = default;

    [[nodiscard]] std::uint16_t channel_id() const noexcept { return _channel_id; }
    [[nodiscard]] synchronization::primitive synchronization_primitive() const noexcept
    {
        return static_cast<synchronization::primitive>(_synchronization_primitive);
    }

    information &operator=(const information &other) = default;

private:
    std::uint16_t _channel_id : 12;
    std::uint16_t _synchronization_primitive : 4;
} __attribute__((packed));

/**
 * Pointer to a resource, stores information about
 * that resource.
 */
class ptr final : public memory::tagged_ptr<void, information>
{
public:
    constexpr ptr() noexcept = default;
    explicit ptr(void *ptr_, const information info = {}) noexcept : memory::tagged_ptr<void, information>(ptr_, info)
    {
    }
    ~ptr() = default;

    ptr &operator=(const ptr &other) noexcept = default;

    [[nodiscard]] std::uint16_t channel_id() const noexcept { return info().channel_id(); }
    [[nodiscard]] synchronization::primitive synchronization_primitive() const noexcept
    {
        return info().synchronization_primitive();
    }
} __attribute__((packed));

/**
 * Casts the internal pointer of the resource pointer
 * to a pointer typed by the given template parameter.
 *
 * @param resource Resource to cast.
 * @return Pointer to the requested type.
 */
template <typename S> static auto *ptr_cast(const ptr resource) noexcept
{
    return resource.template get<S>();
}

} // namespace mx::resource