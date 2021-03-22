#pragma once
#include "synchronization.h"
#include <algorithm>
#include <cstdint>
#include <mx/resource/resource.h>

namespace mx::synchronization {
class PrimitiveMatrix
{
public:
    static primitive select_primitive(const isolation_level isolation_level,
                                      const resource::hint::expected_access_frequency access_frequency,
                                      const resource::hint::expected_read_write_ratio read_write_ratio) noexcept
    {
        return isolation_level != isolation_level::None
                   ? matrix()[static_cast<std::uint8_t>(isolation_level)][static_cast<std::uint8_t>(read_write_ratio)]
                             [static_cast<std::uint8_t>(access_frequency)]
                   : primitive::None;
    }

private:
    constexpr static std::array<std::array<std::array<primitive, 4>, 5>, 2> matrix() noexcept
    {
        return {{// For isolation_level::ExclusiveWriter
                 {{
                     // For predicted_read_write_ratio::heavy_read
                     {{primitive::ScheduleWriter, primitive::ScheduleWriter, primitive::ScheduleWriter,
                       primitive::ScheduleWriter}},

                     // For predicted_read_write_ratio::mostly_read
                     {{primitive::ScheduleWriter, primitive::ScheduleWriter, primitive::OLFIT, primitive::OLFIT}},

                     // For predicted_read_write_ratio::balanced
                     {{primitive::OLFIT, primitive::OLFIT, primitive::OLFIT, primitive::OLFIT}},

                     // For predicted_read_write_ratio::mostly_written
                     {{primitive::OLFIT, primitive::OLFIT, primitive::ReaderWriterLatch, primitive::ReaderWriterLatch}},

                     // For predicted_read_write_ratio::heavy_written
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ReaderWriterLatch,
                       primitive::ReaderWriterLatch}},
                 }},

                 // For isolation_level::Exclusive
                 {{
                     // For predicted_read_write_ratio::heavy_read
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ExclusiveLatch,
                       primitive::ExclusiveLatch}},

                     // For predicted_read_write_ratio::mostly_read
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ExclusiveLatch,
                       primitive::ExclusiveLatch}},

                     // For predicted_read_write_ratio::balanced
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ExclusiveLatch,
                       primitive::ExclusiveLatch}},

                     // For predicted_read_write_ratio::mostly_written
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ExclusiveLatch,
                       primitive::ExclusiveLatch}},

                     // For predicted_read_write_ratio::heavy_written
                     {{primitive::ScheduleAll, primitive::ScheduleAll, primitive::ExclusiveLatch,
                       primitive::ExclusiveLatch}},
                 }}}};
    }
};
} // namespace mx::synchronization