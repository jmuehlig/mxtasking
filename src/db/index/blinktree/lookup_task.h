#pragma once

#include "b_link_tree.h"
#include "insert_separator_task.h"
#include "node.h"
#include "task.h"
#include <optional>

namespace db::index::blinktree {
template <typename K, typename V, class L> class LookupTask final : public Task<K, V, L>
{
public:
    LookupTask(const K key, L &listener) noexcept : Task<K, V, L>(key, listener) {}

    ~LookupTask() override { this->_listener.found(_core_id, this->_key, _value); }

    mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

private:
    V _value;
    std::uint16_t _core_id{0U};
};

template <typename K, typename V, typename L>
mx::tasking::TaskResult LookupTask<K, V, L>::execute(const std::uint16_t core_id, const std::uint16_t /*channel_id*/)
{
    auto *annotated_node = this->annotated_resource().template get<Node<K, V>>();

    // Is the node related to the key?
    if (annotated_node->high_key() <= this->_key)
    {
        this->annotate(annotated_node->right_sibling(), config::node_size() / 4U);
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // If we are accessing an inner node, pick the next related child.
    if (annotated_node->is_inner())
    {
        const auto child = annotated_node->child(this->_key);
        this->annotate(child, config::node_size() / 4U);
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // We are accessing the correct leaf.
    const auto index = annotated_node->index(this->_key);
    if (annotated_node->leaf_key(index) == this->_key)
    {
        this->_value = annotated_node->value(index);
    }
    _core_id = core_id;

    return mx::tasking::TaskResult::make_remove();
}
} // namespace db::index::blinktree
