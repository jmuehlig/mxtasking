#pragma once

#include <array>
#include <functional>
#include <mx/memory/global_heap.h>
#include <mx/tasking/task.h>
#include <mx/util/aligned_t.h>
#include <mx/util/core_set.h>
#include <mx/util/vector.h>
#include <utility>

namespace application::hash_join {
class Benchmark;
class MergeTask final : public mx::tasking::TaskInterface
{
public:
    MergeTask(const mx::util::core_set &cores, Benchmark *benchmark, std::uint64_t output_per_core);
    ~MergeTask() override;

    mx::tasking::TaskResult execute(std::uint16_t /*core_id*/, std::uint16_t /*channel_id*/) override;

    mx::util::vector<std::pair<std::size_t, std::size_t>> &result_set(const std::uint16_t channel_id)
    {
        return _result_sets[channel_id].value();
    }

    [[nodiscard]] const mx::util::vector<std::pair<std::size_t, std::size_t>> &result_set(
        const std::uint16_t channel_id) const
    {
        return _result_sets[channel_id].value();
    }

    [[nodiscard]] std::size_t count_tuples() const noexcept { return _count_output_tuples; }

private:
    Benchmark *_benchmark;
    const std::uint16_t _count_cores;
    std::size_t _count_output_tuples{0U};
    mx::util::aligned_t<mx::util::vector<std::pair<std::size_t, std::size_t>>> *_result_sets;
};
} // namespace application::hash_join