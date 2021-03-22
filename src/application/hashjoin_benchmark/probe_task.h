#pragma once

#include "inline_hashtable.h"
#include <iostream>
#include <mx/tasking/task.h>
#include <mx/util/vector.h>
#include <utility>
#include <vector>

namespace application::hash_join {
class ProbeTask final : public mx::tasking::TaskInterface
{
public:
    ProbeTask(mx::util::vector<std::pair<std::size_t, std::size_t>> &result_set, const std::size_t size,
              const std::uint8_t /*numa_node_id*/)
        : _result_set(result_set)
    {
        _keys.reserve(size);
    }

    ~ProbeTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/) override
    {
        auto *hashtable = this->annotated_resource().get<InlineHashtable<std::uint32_t, std::size_t>>();

        for (const auto &[row_id, key] : _keys)
        {
            const auto row = hashtable->get(key);
            if (row != std::numeric_limits<std::size_t>::max())
            {
                _result_set.emplace_back(std::make_pair(row_id, row));
            }
        }

        return mx::tasking::TaskResult::make_remove();
    }

    void emplace_back(const std::size_t row_id, const std::uint32_t key) noexcept
    {
        _keys.emplace_back(std::make_pair(row_id, key));
    }

    [[nodiscard]] std::uint64_t size() const noexcept { return _keys.size(); }
    [[nodiscard]] bool empty() const noexcept { return _keys.empty(); }

private:
    std::vector<std::pair<std::size_t, std::uint32_t>> _keys;
    mx::util::vector<std::pair<std::size_t, std::size_t>> &_result_set;
};
} // namespace application::hash_join