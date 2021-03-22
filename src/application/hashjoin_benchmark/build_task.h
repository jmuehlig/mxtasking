#pragma once
#include "inline_hashtable.h"
#include <cstdint>
#include <iostream>
#include <mx/tasking/task.h>
#include <vector>

namespace application::hash_join {
/**
 * The build task builds the hash table.
 */
class BuildTask final : public mx::tasking::TaskInterface
{
public:
    BuildTask(const std::size_t size, const std::uint8_t /*numa_node_id*/) { _keys.reserve(size); }
    ~BuildTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/) override
    {
        auto *hashtable = this->annotated_resource().get<InlineHashtable<std::uint32_t, std::size_t>>();

        for (const auto &row : _keys)
        {
            hashtable->insert(std::get<1>(row), std::get<0>(row));
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
    // Keys and row ids to insert into the hashtable.
    std::vector<std::pair<std::size_t, std::uint32_t>> _keys;
};
} // namespace application::hash_join