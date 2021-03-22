#pragma once

namespace application::blinktree_benchmark {
class config
{
public:
    /**
     * @return Number of requests that will be started at a time by the request scheduler.
     */
    static constexpr auto batch_size() noexcept { return 500U; }

    /**
     * @return Number of maximal open requests, system-wide.
     */
    static constexpr auto max_parallel_requests() noexcept { return 1500U; }
};
} // namespace application::blinktree_benchmark