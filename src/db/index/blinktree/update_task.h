#pragma once

#include "b_link_tree.h"
#include "insert_separator_task.h"
#include "node.h"
#include "task.h"
#include <iostream>

namespace db::index::blinktree {
template <typename K, typename V, class L> class UpdateTask final : public Task<K, V, L>
{
public:
    constexpr UpdateTask(const K key, const V value, L &listener) noexcept : Task<K, V, L>(key, listener), _value(value)
    {
    }

    ~UpdateTask() override = default;

    mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

private:
    const V _value;
};

template <typename K, typename V, typename L>
mx::tasking::TaskResult UpdateTask<K, V, L>::execute(const std::uint16_t core_id, const std::uint16_t /*channel_id*/)
{
    auto *node = this->annotated_resource().template get<Node<K, V>>();

    // Is the node related to the key?
    if (node->high_key() <= this->_key)
    {
        this->annotate(node->right_sibling(), config::node_size() / 4U);
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // If we are accessing an inner node, pick the next related child.
    if (node->is_inner())
    {
        const auto child = node->child(this->_key);
        this->annotate(child, config::node_size() / 4U);
        this->is_readonly(!node->is_branch());
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // If the task is still reading, but this is a leaf,
    // spawn again as writer.
    if (node->is_leaf() && this->is_readonly())
    {
        this->is_readonly(false);
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // We are accessing the correct leaf.
    const auto index = node->index(this->_key);
    const auto key = node->leaf_key(index);
    if (key == this->_key)
    {
        node->value(index, this->_value);
        this->_listener.updated(core_id, key, this->_value);
    }
    else
    {
        this->_listener.missing(core_id, key);
    }

    return mx::tasking::TaskResult::make_remove();
}
} // namespace db::index::blinktree
