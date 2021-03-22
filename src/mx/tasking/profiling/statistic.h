#pragma once
#include <array>
#include <cstdint>
#include <mx/memory/global_heap.h>
#include <mx/tasking/config.h>
#include <mx/util/aligned_t.h>

namespace mx::tasking::profiling {
/**
 * Collector for tasking statistics (scheduled tasks, executed tasks, ...).
 */
class Statistic
{
public:
    using counter_line_t = util::aligned_t<std::array<std::uint64_t, 7>>;

    enum Counter : std::uint8_t
    {
        Scheduled,
        ScheduledOnChannel,
        ScheduledOffChannel,
        Executed,
        ExecutedReader,
        ExecutedWriter,
        Fill
    };

    explicit Statistic(const std::uint16_t count_channels) noexcept : _count_channels(count_channels)
    {
        this->_counter = new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(counter_line_t) * count_channels))
            counter_line_t[count_channels];
        std::memset(static_cast<void *>(this->_counter), 0, sizeof(counter_line_t) * count_channels);
    }

    Statistic(const Statistic &) = delete;

    ~Statistic() noexcept { delete[] this->_counter; }

    Statistic &operator=(const Statistic &) = delete;

    /**
     * Clears all collected statistics.
     */
    void clear() noexcept
    {
        std::memset(static_cast<void *>(this->_counter), 0, sizeof(counter_line_t) * this->_count_channels);
    }

    /**
     * Increment the template-given counter by one for the given channel.
     * @param channel_id Channel to increment the statistics for.
     */
    template <Counter C> void increment(const std::uint16_t channel_id) noexcept
    {
        _counter[channel_id].value()[static_cast<std::uint8_t>(C)] += 1;
    }

    /**
     * Read the given counter for a given channel.
     * @param counter Counter to read.
     * @param channel_id Channel the counter is for.
     * @return Value of the counter.
     */
    [[nodiscard]] std::uint64_t get(const Counter counter, const std::uint16_t channel_id) const noexcept
    {
        return _counter[channel_id].value()[static_cast<std::uint8_t>(counter)];
    }

    /**
     * Read and aggregate the counter for all channels.
     * @param counter Counter to read.
     * @return Value of the counter for all channels.
     */
    [[nodiscard]] std::uint64_t get(const Counter counter) const noexcept
    {
        std::uint64_t sum = 0U;
        for (auto i = 0U; i < _count_channels; ++i)
        {
            sum += get(counter, i);
        }

        return sum;
    }

private:
    // Number of channels to monitor.
    const std::uint16_t _count_channels;

    // Memory for storing the counter.
    counter_line_t *_counter = nullptr;
};
} // namespace mx::tasking::profiling