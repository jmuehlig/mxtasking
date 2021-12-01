#pragma once

namespace mx::tasking {
class config
{
public:
    enum memory_reclamation_scheme
    {
        None = 0U,
        UpdateEpochOnRead = 1U,
        UpdateEpochPeriodically = 2U
    };

    // Maximal number of supported cores.
    static constexpr auto max_cores() { return 128U; }

    // Maximal size for a single task, will be used for task allocation.
    static constexpr auto task_size() { return 64U; }

    // The task buffer will hold a set of tasks, fetched from
    // queues. This is the size of the buffer.
    static constexpr auto task_buffer_size() { return 64U; }

    // If enabled, will record the number of execute tasks,
    // scheduled tasks, reader and writer per core and more.
    static constexpr auto task_statistics() { return false; }

    // If enabled, memory will be reclaimed while using optimistic
    // synchronization by epoch-based reclamation. Otherwise, freeing
    // memory is unsafe.
    static constexpr auto memory_reclamation() { return memory_reclamation_scheme::UpdateEpochPeriodically; }
};
} // namespace mx::tasking
