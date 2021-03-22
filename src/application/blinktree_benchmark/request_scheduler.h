#pragma once

#include "config.h"
#include "listener.h"
#include <atomic>
#include <benchmark/workload.h>
#include <cstdint>
#include <db/index/blinktree/b_link_tree.h>
#include <db/index/blinktree/config.h>
#include <db/index/blinktree/insert_value_task.h>
#include <db/index/blinktree/lookup_task.h>
#include <db/index/blinktree/update_task.h>
#include <mx/resource/resource.h>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <mx/util/core_set.h>
#include <mx/util/reference_counter.h>

namespace application::blinktree_benchmark {

class RequestIndex
{
public:
    static RequestIndex make_finished() { return RequestIndex{std::numeric_limits<decltype(_index)>::max(), 0UL}; }
    static RequestIndex make_no_new() { return RequestIndex{0UL, 0UL}; }

    RequestIndex(const std::uint64_t index, const std::uint64_t count) noexcept : _index(index), _count(count) {}
    explicit RequestIndex(std::pair<std::uint64_t, std::uint64_t> &&index_and_count) noexcept
        : _index(std::get<0>(index_and_count)), _count(std::get<1>(index_and_count))
    {
    }
    RequestIndex(RequestIndex &&) noexcept = default;
    RequestIndex(const RequestIndex &) = default;
    ~RequestIndex() noexcept = default;

    RequestIndex &operator=(RequestIndex &&) noexcept = default;

    [[nodiscard]] std::uint64_t index() const noexcept { return _index; }
    [[nodiscard]] std::uint64_t count() const noexcept { return _count; }

    [[nodiscard]] bool is_finished() const noexcept { return _index == std::numeric_limits<decltype(_index)>::max(); }
    [[nodiscard]] bool has_new() const noexcept { return _count > 0UL; }

    RequestIndex &operator-=(const std::uint64_t count) noexcept
    {
        _count -= count;
        _index += count;
        return *this;
    }

private:
    std::uint64_t _index;
    std::uint64_t _count;
};

/**
 * The RequestContainer manages the workload and allocates new batches of requests
 * that will be scheduled by the request scheduler.
 */
class RequestContainer
{
public:
    RequestContainer(const std::uint16_t core_id, const std::uint64_t max_open_requests,
                     benchmark::Workload &workload) noexcept
        : _finished_requests(core_id), _local_buffer(workload.next(config::batch_size())),
          _max_pending_requests(max_open_requests), _workload(workload)
    {
    }

    ~RequestContainer() noexcept = default;

    /**
     * Allocates the next requests to spawn.
     *
     * @return Pair of workload-index and number of tuples to request.
     *         When the number is negative, no more requests are available.
     */
    RequestIndex next() noexcept
    {
        const auto finished_requests = _finished_requests.load();

        const auto pending_requests = _scheduled_requests - finished_requests;
        if (pending_requests >= _max_pending_requests)
        {
            // Too many open requests somewhere in the system.
            return RequestIndex::make_no_new();
        }

        if (_local_buffer.has_new() == false)
        {
            _local_buffer = RequestIndex{_workload.next(config::batch_size())};
        }

        if (_local_buffer.has_new())
        {
            // How many requests can be scheduled without reaching the request limit?
            const auto free_requests = _max_pending_requests - pending_requests;

            // Try to spawn all free requests, but at least those in the local buffer.
            const auto count = std::min(free_requests, _local_buffer.count());

            _scheduled_requests += count;

            const auto index = RequestIndex{_local_buffer.index(), count};
            _local_buffer -= count;

            return index;
        }

        // Do we have to wait for pending requests or are we finished?
        return pending_requests > 0UL ? RequestIndex::make_no_new() : RequestIndex::make_finished();
    }

    /**
     * Callback after inserted a value.
     */
    void inserted(const std::uint16_t core_id, const std::uint64_t /*key*/, const std::int64_t /*value*/) noexcept
    {
        task_finished(core_id);
    }

    /**
     * Callback after updated a value.
     */
    void updated(const std::uint16_t core_id, const std::uint64_t /*key*/, const std::int64_t /*value*/) noexcept
    {
        task_finished(core_id);
    }

    /**
     * Callback after removed a value.
     */
    void removed(const std::uint16_t core_id, const std::uint64_t /*key*/) noexcept { task_finished(core_id); }

    /**
     * Callback after found a value.
     */
    void found(const std::uint16_t core_id, const std::uint64_t /*key*/, const std::int64_t /*value*/) noexcept
    {
        task_finished(core_id);
    }

    /**
     * Callback on missing a value.
     */
    void missing(const std::uint16_t core_id, const std::uint64_t /*key*/) noexcept { task_finished(core_id); }

    const benchmark::NumericTuple &operator[](const std::size_t index) const noexcept { return _workload[index]; }

private:
    // Number of requests finished by tasks.
    mx::util::reference_counter_64 _finished_requests;

    // Number of tasks scheduled by the owning request scheduler.
    std::uint64_t _scheduled_requests = 0UL;

    // Local buffer holding not scheduled, but from global worker owned request items.
    RequestIndex _local_buffer;

    // Number of requests that can be distributed by this scheduler,
    // due to system-wide maximal parallel requests.
    const std::uint64_t _max_pending_requests;

    // Workload to get requests from.
    benchmark::Workload &_workload;

    /**
     * Updates the counter of finished requests.
     */
    void task_finished(const std::uint16_t core_id) { _finished_requests.add(core_id); }
};

/**
 * The RequestScheduler own its own request container and sets up requests for the BLink-Tree.
 */
class RequestSchedulerTask final : public mx::tasking::TaskInterface
{
public:
    RequestSchedulerTask(const std::uint16_t core_id, const std::uint16_t channel_id, benchmark::Workload &workload,
                         const mx::util::core_set &core_set,
                         db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t> *tree, Listener *listener)
        : _tree(tree), _listener(listener)
    {
        this->annotate(mx::tasking::priority::low);
        this->is_readonly(false);

        const auto container = mx::tasking::runtime::new_resource<RequestContainer>(
            sizeof(RequestContainer), mx::resource::hint{channel_id}, core_id,
            config::max_parallel_requests() / core_set.size(), workload);
        this->annotate(container, sizeof(RequestContainer));
    }

    ~RequestSchedulerTask() final = default;

    mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t channel_id) override
    {
        // Get some new requests from the container.
        auto &request_container = *mx::resource::ptr_cast<RequestContainer>(this->annotated_resource());
        const auto next_requests = request_container.next();

        if (next_requests.has_new())
        {
            for (auto i = next_requests.index(); i < next_requests.index() + next_requests.count(); ++i)
            {
                mx::tasking::TaskInterface *task{nullptr};
                const auto &tuple = request_container[i];
                if (tuple == benchmark::NumericTuple::INSERT)
                {
                    task = mx::tasking::runtime::new_task<
                        db::index::blinktree::InsertValueTask<std::uint64_t, std::int64_t, RequestContainer>>(
                        core_id, tuple.key(), tuple.value(), _tree, request_container);
                    task->is_readonly(_tree->height() > 1U);
                }
                else if (tuple == benchmark::NumericTuple::LOOKUP)
                {
                    task = mx::tasking::runtime::new_task<
                        db::index::blinktree::LookupTask<std::uint64_t, std::int64_t, RequestContainer>>(
                        core_id, tuple.key(), request_container);

                    task->is_readonly(true);
                }
                else if (tuple == benchmark::NumericTuple::UPDATE)
                {
                    task = mx::tasking::runtime::new_task<
                        db::index::blinktree::UpdateTask<std::uint64_t, std::int64_t, RequestContainer>>(
                        core_id, tuple.key(), tuple.value(), request_container);
                    task->is_readonly(_tree->height() > 1U);
                }

                task->annotate(_tree->root(), db::index::blinktree::config::node_size() / 4U);
                mx::tasking::runtime::spawn(*task, channel_id);
            }
        }
        else if (next_requests.is_finished())
        {
            // All requests are done. Notify the benchmark and die.
            _listener->requests_finished();
            mx::tasking::runtime::delete_resource<RequestContainer>(this->annotated_resource());
            return mx::tasking::TaskResult::make_remove();
        }

        return mx::tasking::TaskResult::make_succeed(this);
    }

private:
    // The tree to send requests to.
    db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t> *_tree;

    // Benchmark listener to notify on requests are done.
    Listener *_listener;
};
} // namespace application::blinktree_benchmark