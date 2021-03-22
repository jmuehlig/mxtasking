#pragma once

namespace application::blinktree_benchmark {
/**
 * The listener will be used to notify the benchmark that request tasks are
 * done and no more work is available.
 */
class Listener
{
public:
    constexpr Listener() = default;
    virtual ~Listener() = default;
    virtual void requests_finished() = 0;
};
} // namespace application::blinktree_benchmark