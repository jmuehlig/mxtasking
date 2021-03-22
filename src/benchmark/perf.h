#pragma once
#include <algorithm>
#include <asm/unistd.h>
#include <cstring>
#include <linux/perf_event.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

/*
 * For more Performance Counter take a look into the Manual from Intel:
 *  https://software.intel.com/sites/default/files/managed/8b/6e/335279_performance_monitoring_events_guide.pdf
 *
 * To get event ids from manual specification see libpfm4:
 *  http://www.bnikolic.co.uk/blog/hpc-prof-events.html
 * Clone, Make, use examples/check_events to generate event id code from event:
 *  ./check_events <category>:<umask>[:c=<cmask>]
 * Example:
 *  ./cycle_activity:0x14:c=20
 */

namespace benchmark {

/**
 * Represents a Linux Performance Counter.
 */
class PerfCounter
{
public:
    PerfCounter(std::string &&name, const std::uint64_t type, const std::uint64_t event_id) : _name(std::move(name))
    {
        std::memset(&_perf_event_attribute, 0, sizeof(perf_event_attr));
        _perf_event_attribute.type = type;
        _perf_event_attribute.size = sizeof(perf_event_attr);
        _perf_event_attribute.config = event_id;
        _perf_event_attribute.disabled = true;
        _perf_event_attribute.inherit = 1;
        _perf_event_attribute.exclude_kernel = false;
        _perf_event_attribute.exclude_hv = false;
        _perf_event_attribute.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    }

    ~PerfCounter() = default;

    bool open()
    {
        _file_descriptor = syscall(__NR_perf_event_open, &_perf_event_attribute, 0, -1, -1, 0);
        return _file_descriptor >= 0;
    }

    bool start()
    {
        ioctl(_file_descriptor, PERF_EVENT_IOC_RESET, 0);
        ioctl(_file_descriptor, PERF_EVENT_IOC_ENABLE, 0);
        return ::read(_file_descriptor, &_prev, sizeof(read_format)) == sizeof(read_format);
    }

    bool stop()
    {
        const auto is_read = ::read(_file_descriptor, &_data, sizeof(read_format)) == sizeof(read_format);
        ioctl(_file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
        return is_read;
    }

    [[nodiscard]] double read() const
    {
        const auto multiplexing_correction = static_cast<double>(_data.time_enabled - _prev.time_enabled) /
                                             static_cast<double>(_data.time_running - _prev.time_running);
        return static_cast<double>(_data.value - _prev.value) * multiplexing_correction;
    }

    [[nodiscard]] const std::string &name() const { return _name; }
    explicit operator const std::string &() const { return name(); }

    bool operator==(const std::string &name) const { return _name == name; }

private:
    struct read_format
    {
        std::uint64_t value = 0;
        std::uint64_t time_enabled = 0;
        std::uint64_t time_running = 0;
    };

    const std::string _name;
    std::int32_t _file_descriptor = -1;
    perf_event_attr _perf_event_attribute{};
    read_format _prev{};
    read_format _data{};
};

/**
 * Holds a set of performance counter and starts/stops them together.
 */
class Perf
{
public:
    [[maybe_unused]] static PerfCounter INSTRUCTIONS;
    [[maybe_unused]] static PerfCounter CYCLES;
    [[maybe_unused]] static PerfCounter L1_MISSES;
    [[maybe_unused]] [[maybe_unused]] static PerfCounter LLC_MISSES;
    [[maybe_unused]] static PerfCounter LLC_REFERENCES;
    [[maybe_unused]] static PerfCounter STALLED_CYCLES_BACKEND;
    [[maybe_unused]] static PerfCounter STALLS_MEM_ANY;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_NTA;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_T0;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_T1_T2;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_WRITE;

    Perf() noexcept = default;
    ~Perf() noexcept = default;

    bool add(PerfCounter &counter_)
    {
        if (counter_.open())
        {
            _counter.push_back(counter_);
            return true;
        }

        return false;
    }

    void start()
    {
        for (auto &counter_ : _counter)
        {
            counter_.start();
        }
    }

    void stop()
    {
        for (auto &counter_ : _counter)
        {
            counter_.stop();
        }
    }

    double operator[](const std::string &name) const
    {
        auto counter_iterator = std::find(_counter.begin(), _counter.end(), name);
        if (counter_iterator != _counter.end())
        {
            return counter_iterator->read();
        }

        return 0.0;
    }

    std::vector<PerfCounter> &counter() { return _counter; }

private:
    std::vector<PerfCounter> _counter;
};
} // namespace benchmark