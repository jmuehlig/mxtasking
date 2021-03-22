#pragma once

#include "config.h"
#include "task_stack.h"
#include <bitset>
#include <cstdint>
#include <functional>
#include <mx/resource/resource.h>
#include <variant>

namespace mx::tasking {
enum priority : std::uint8_t
{
    low = 0,
    normal = 1
};

class TaskInterface;
class TaskResult
{
public:
    static TaskResult make_succeed(TaskInterface *successor_task) noexcept { return TaskResult{successor_task, false}; }
    static TaskResult make_remove() noexcept { return TaskResult{nullptr, true}; }
    static TaskResult make_succeed_and_remove(TaskInterface *successor_task) noexcept
    {
        return TaskResult{successor_task, true};
    }
    static TaskResult make_null() noexcept { return TaskResult{nullptr, false}; }
    constexpr TaskResult() = default;
    ~TaskResult() = default;

    TaskResult &operator=(const TaskResult &) = default;

    explicit operator TaskInterface *() const noexcept { return _successor_task; }

    [[nodiscard]] bool is_remove() const noexcept { return _remove_task; }
    [[nodiscard]] bool has_successor() const noexcept { return _successor_task != nullptr; }

private:
    constexpr TaskResult(TaskInterface *successor_task, const bool remove) noexcept
        : _successor_task(successor_task), _remove_task(remove)
    {
    }
    TaskInterface *_successor_task = nullptr;
    bool _remove_task = false;
};

/**
 * The task is the central execution unit of mxtasking.
 * Every task that should be executed has to derive
 * from this class.
 */
class TaskInterface
{
public:
    using channel = std::uint16_t;
    using node = std::uint8_t;
    using resource_and_size = std::pair<mx::resource::ptr, std::uint16_t>;

    constexpr TaskInterface() = default;
    virtual ~TaskInterface() = default;

    /**
     * Will be executed by a worker when the task gets CPU time.
     *
     * @param core_id       (System-)ID of the core, the task is executed on.
     * @param channel_id    Channel ID the task is executed on.
     * @return Pointer to the follow up task.
     */
    virtual TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) = 0;

    /**
     * Annotate the task with a resource the task will work on.
     *
     * @param resource Pointer to the resource.
     * @param size  Size of the resource (that will be prefetched).
     */
    void annotate(const mx::resource::ptr resource_, const std::uint16_t size) noexcept
    {
        _annotation.target = std::make_pair(resource_, size);
    }

    /**
     * Annotate the task with a desired channel the task should be executed on.
     *
     * @param channel_id ID of the channel.
     */
    void annotate(const channel channel_id) noexcept { _annotation.target = channel_id; }

    /**
     * Annotate the task with a desired NUMA node id the task should executed on.
     *
     * @param node_id ID of the NUMA node.
     */
    void annotate(const node node_id) noexcept { _annotation.target = node_id; }

    /**
     * Annotate the task with a run priority (low, normal, high).
     *
     * @param priority_ Priority the task should run with.
     */
    void annotate(const priority priority_) noexcept { _annotation.priority = priority_; }

    /**
     * Annotate the task whether it is a reading or writing task.
     *
     * @param is_readonly True, when the task is read only (false by default).
     */
    void is_readonly(const bool is_readonly) noexcept { _annotation.is_readonly = is_readonly; }

    /**
     * @return The annotated resource.
     */
    [[nodiscard]] mx::resource::ptr annotated_resource() const noexcept
    {
        return std::get<0>(std::get<resource_and_size>(_annotation.target));
    }

    /**
     * @return The annotated resource size.
     */
    [[nodiscard]] std::uint16_t annotated_resource_size() const noexcept
    {
        return std::get<1>(std::get<resource_and_size>(_annotation.target));
    }

    /**
     * @return The annotated channel.
     */
    [[nodiscard]] channel annotated_channel() const noexcept { return std::get<channel>(_annotation.target); }

    /**
     * @return The annotated NUMA node id.
     */
    [[nodiscard]] node annotated_node() const noexcept { return std::get<node>(_annotation.target); }

    /**
     * @return Annotated priority.
     */
    [[nodiscard]] enum priority priority() const noexcept { return _annotation.priority; }

    /**
     * @return True, when the task is a read only task.
     */
    [[nodiscard]] bool is_readonly() const noexcept { return _annotation.is_readonly; }

    /**
     * @return True, when the task has a resource annotated.
     */
    [[nodiscard]] bool has_resource_annotated() const noexcept
    {
        return std::holds_alternative<resource_and_size>(_annotation.target);
    }

    /**
     * @return True, when the task has a channel annotated.
     */
    [[nodiscard]] bool has_channel_annotated() const noexcept
    {
        return std::holds_alternative<channel>(_annotation.target);
    }

    /**
     * @return True, when the task has a NUMA node annotated.
     */
    [[nodiscard]] bool has_node_annotated() const noexcept { return std::holds_alternative<node>(_annotation.target); }

    /**
     * @return Pointer to the next task in spawn queue.
     */
    [[nodiscard]] TaskInterface *next() const noexcept { return _next; }

    /**
     * Set the next task for scheduling.
     * @param next Task scheduled after this task.
     */
    void next(TaskInterface *next) noexcept { _next = next; }

private:
    /**
     * Annotation of a task.
     */
    class annotation
    {
    public:
        constexpr annotation() noexcept = default;
        ~annotation() = default;

        // Is the task just reading?
        bool is_readonly{false};

        // Priority of a task.
        enum priority priority
        {
            priority::normal
        };

        // Target the task will run on.
        std::variant<channel, node, resource_and_size, bool> target{false};
    } __attribute__((packed));

    // Pointer for next task in queue.
    TaskInterface *_next{nullptr};

    // Tasks annotations.
    annotation _annotation;
};
} // namespace mx::tasking