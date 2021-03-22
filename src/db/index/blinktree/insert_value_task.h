#pragma once

#include "b_link_tree.h"
#include "insert_separator_task.h"
#include "node.h"
#include "task.h"
#include <mx/tasking/runtime.h>
#include <vector>

namespace db::index::blinktree {
template <typename K, typename V, class L> class InsertValueTask final : public Task<K, V, L>
{
public:
    constexpr InsertValueTask(const K key, const V value, BLinkTree<K, V> *tree, L &listener) noexcept
        : Task<K, V, L>(key, listener), _tree(tree), _value(value)
    {
    }

    ~InsertValueTask() override = default;

    mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

private:
    BLinkTree<K, V> *_tree;
    const V _value;
};

template <typename K, typename V, class L>
mx::tasking::TaskResult InsertValueTask<K, V, L>::execute(const std::uint16_t core_id,
                                                          const std::uint16_t /*channel_id*/)
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
        this->is_readonly(!annotated_node->is_branch());
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // Is it a leaf, but we are still reading? Upgrade to writer.
    if (annotated_node->is_leaf() && this->is_readonly())
    {
        this->is_readonly(false);
        return mx::tasking::TaskResult::make_succeed(this);
    }

    // At this point, we are accessing the related leaf and we are in writer mode.
    const auto index = annotated_node->index(this->_key);
    if (index < annotated_node->size() && annotated_node->leaf_key(index) == this->_key)
    {
        this->_listener.inserted(core_id, this->_key, this->_value);
        return mx::tasking::TaskResult::make_remove();
    }

    if (annotated_node->full() == false)
    {
        annotated_node->insert(index, this->_value, this->_key);
        this->_listener.inserted(core_id, this->_key, this->_value);
        return mx::tasking::TaskResult::make_remove();
    }

    auto [right, key] = this->_tree->split(this->annotated_resource(), this->_key, this->_value);
    if (annotated_node->parent() != nullptr)
    {
        auto *task = mx::tasking::runtime::new_task<InsertSeparatorTask<K, V, L>>(core_id, key, right, this->_tree,
                                                                                  this->_listener);
        task->annotate(annotated_node->parent(), config::node_size() / 4U);
        return mx::tasking::TaskResult::make_succeed_and_remove(task);
    }

    this->_tree->create_new_root(this->annotated_resource(), right, key);
    this->_listener.inserted(core_id, this->_key, this->_value);
    return mx::tasking::TaskResult::make_remove();
}
} // namespace db::index::blinktree
