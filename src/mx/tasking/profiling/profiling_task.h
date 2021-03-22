#pragma once

#include <chrono>
#include <mx/tasking/channel.h>
#include <mx/tasking/task.h>
#include <mx/util/maybe_atomic.h>
#include <optional>
#include <utility>
#include <vector>

namespace mx::tasking::profiling {
/**
 * Time range (from -- to) for idled time of a single channel.
 */
class IdleRange
{
public:
    IdleRange() : _start(std::chrono::steady_clock::now()) {}
    IdleRange(IdleRange &&) = default;
    ~IdleRange() = default;

    /**
     * Sets the end of the idle range to the current time.
     */
    void stop() noexcept { _end = std::chrono::steady_clock::now(); }

    /**
     * @return Number of nanoseconds idled.
     */
    [[nodiscard]] std::uint64_t nanoseconds() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(_end - _start).count();
    }

    /**
     * Normalizes this range with respect to a given point in time.
     * @param global_start Point in time to normalize.
     * @return Pair of (start, stop) normalized to the given time point.
     */
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> normalize(
        const std::chrono::steady_clock::time_point global_start) const noexcept
    {
        return {
            std::chrono::duration_cast<std::chrono::nanoseconds>(_start - global_start).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(_end - global_start).count(),
        };
    }

private:
    // Start of idling.
    std::chrono::steady_clock::time_point _start;

    // End of idling.
    std::chrono::steady_clock::time_point _end;
};

/**
 * Task, that is scheduled with low priority and gets CPU time,
 * whenever no other task is available.
 * Every time the task gets executed, it will record the time range,
 * until the channel has new tasks for execution.
 */
class ProfilingTask final : public TaskInterface
{
public:
    ProfilingTask(util::maybe_atomic<bool> &is_running, Channel &channel);
    ~ProfilingTask() override = default;

    TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

    [[nodiscard]] const std::vector<IdleRange> &idle_ranges() const noexcept { return _idle_ranges; }

private:
    util::maybe_atomic<bool> &_is_running;
    Channel &_channel;
    std::vector<IdleRange> _idle_ranges;
};

/**
 * Schedules the idle/profiling task to every channel and
 * writes the memory to a given file.
 */
class Profiler
{
public:
    Profiler() noexcept = default;
    ~Profiler();

    /**
     * Enable profiling and set the result file.
     * @param profiling_output_file File, where results should be written to.
     */
    void profile(const std::string &profiling_output_file);

    /**
     * Schedules a new idle/profile task to the given channel.
     * @param is_running Reference to the schedulers "is_running" flag.
     * @param channel   Channel to spawn the task to.
     */
    void profile(util::maybe_atomic<bool> &is_running, Channel &channel);

    /**
     * Normalizes all time ranges and writes them to the specified
     * file.
     */
    void stop();

private:
    // File to write the output.
    std::optional<std::string> _profiling_output_file{std::nullopt};

    // Time point of the runtime start.
    std::chrono::steady_clock::time_point _start;

    // List of all idle/profile tasks.
    std::vector<ProfilingTask *> _tasks;
};

} // namespace mx::tasking::profiling