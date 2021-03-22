#include "builder.h"
#include <mx/synchronization/primitive_matrix.h>

using namespace mx::resource;

std::pair<std::uint16_t, std::uint8_t> Builder::schedule(const resource::hint &hint)
{
    // Scheduling was done by the hint.
    if (hint.has_channel_id())
    {
        this->_scheduler.predict_usage(hint.channel_id(), hint.access_frequency());
        return std::make_pair(hint.channel_id(), this->_scheduler.numa_node_id(hint.channel_id()));
    }

    // Schedule resources round robin to the channels.
    const auto count_channels = this->_scheduler.count_channels();
    auto channel_id = this->_round_robin_channel_id.fetch_add(1U, std::memory_order_relaxed) % count_channels;

    // If the chosen channel contains an excessive accessed resource, get another.
    if (count_channels > 2U && hint.isolation_level() == synchronization::isolation_level::Exclusive &&
        this->_scheduler.has_excessive_usage_prediction(channel_id))
    {
        channel_id = this->_round_robin_channel_id.fetch_add(1U, std::memory_order_relaxed) % count_channels;
    }
    this->_scheduler.predict_usage(channel_id, hint.access_frequency());

    const auto numa_node_id = hint.has_numa_node_id() ? hint.numa_node_id() : this->_scheduler.numa_node_id(channel_id);

    return std::make_pair(channel_id, numa_node_id);
}

mx::synchronization::primitive Builder::isolation_level_to_synchronization_primitive(const hint &hint) noexcept
{
    // The developer did not define any fixed protocol for
    // synchronization; we choose one depending on the hints.
    if (hint == synchronization::protocol::None)
    {
        return synchronization::PrimitiveMatrix::select_primitive(hint.isolation_level(), hint.access_frequency(),
                                                                  hint.read_write_ratio());
    }

    // The developer hinted a specific protocol (latched, queued, ...)
    // and a relaxed isolation level.
    if (hint == synchronization::isolation_level::ExclusiveWriter)
    {
        switch (hint.preferred_protocol())
        {
        case synchronization::protocol::Latch:
            return synchronization::primitive::ReaderWriterLatch;
        case synchronization::protocol::OLFIT:
            return synchronization::primitive::OLFIT;
        default:
            return synchronization::primitive::ScheduleWriter;
        }
    }

    // The developer hinted a specific protocol (latched, queued, ...)
    // and a strict isolation level.
    if (hint == synchronization::isolation_level::Exclusive)
    {
        return hint == synchronization::protocol::Latch ? synchronization::primitive::ExclusiveLatch
                                                        : synchronization::primitive::ScheduleAll;
    }

    return mx::synchronization::primitive::None;
}