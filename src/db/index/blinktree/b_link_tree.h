#pragma once

#include "config.h"
#include "node.h"
#include "node_consistency_checker.h"
#include "node_iterator.h"
#include "node_statistics.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <mx/resource/resource.h>
#include <mx/tasking/runtime.h>
#include <utility>
#include <vector>

namespace db::index::blinktree {

template <typename K, typename V> class BLinkTree
{
public:
    BLinkTree(const mx::synchronization::isolation_level isolation_level,
              const mx::synchronization::protocol preferred_synchronization_protocol)
        : _isolation_level(isolation_level), _preferred_synchronization_protocol(preferred_synchronization_protocol),
          _root(create_node(NodeType::Leaf, mx::resource::ptr{}, true))
    {
    }

    ~BLinkTree() { mx::tasking::runtime::delete_resource<Node<K, V>>(_root); }

    /**
     * @return Root node of the tree.
     */
    [[nodiscard]] mx::resource::ptr root() const { return _root; }

    /**
     * @return Height of the tree.
     */
    [[nodiscard]] std::uint16_t height() const { return _height; }

    /**
     * @return True, when the tree does not contain any value.
     */
    [[nodiscard]] bool empty() const
    {
        return static_cast<bool>(_root) == false || _root.template get<Node<K, V>>()->size() == 0;
    }

    /**
     * Creates a node of type inner.
     *
     * @param is_branch True, when the children of the new inner node will be leaf nodes.
     * @param parent Parent of the new inner node.
     * @param is_root True, then the new inner node will be the root.
     * @return Inner node.
     */
    [[nodiscard]] mx::resource::ptr create_inner_node(const bool is_branch, const mx::resource::ptr parent,
                                                      const bool is_root = false) const
    {
        const auto inner_type = is_branch ? NodeType::Inner | NodeType::Branch : NodeType::Inner;
        return create_node(inner_type, parent, is_root);
    }

    /**
     * Creates a node of type leaf.
     *
     * @param parent Parent of the new leaf node.
     * @return Leaf node.
     */
    [[nodiscard]] mx::resource::ptr create_leaf_node(const mx::resource::ptr parent) const
    {
        return create_node(NodeType::Leaf, parent, false);
    }

    /**
     * Creates a new root node, containing two separators (to the left and right).
     * The new root node will be set in the tree.
     *
     * @param left Link to the "smaller" child node.
     * @param right Link to the "greater" child node.
     * @param key Separator key.
     */
    void create_new_root(mx::resource::ptr left, mx::resource::ptr right, K key);

    /**
     * Splits an inner node.
     *
     * @param inner_node Node to split.
     * @param key Key to insert after split.
     * @param separator Separator to insert after split.
     * @return Pointer and high key of the new node.
     */
    std::pair<mx::resource::ptr, K> split(mx::resource::ptr inner_node, K key, mx::resource::ptr separator) const;

    /**
     * Splits a leaf node.
     *
     * @param leaf_node Node to split.
     * @param key Key to insert after split.
     * @param value Value to insert after split.
     * @return Pointer to the leaf node and key for parent.
     */
    std::pair<mx::resource::ptr, K> split(mx::resource::ptr leaf_node, K key, V value) const;

    /**
     * @return Begin iterator for iterating ofer nodes.
     */
    NodeIterator<K, V> begin() const { return NodeIterator(mx::resource::ptr_cast<Node<K, V>>(_root)); }

    /**
     * @return End iterator (aka empty node iterator).
     */
    NodeIterator<K, V> end() const { return {}; }

    /**
     * Checks the consistency of the tree.
     */
    void check() const;

    /**
     * Dumps the statistics like height, number of (inner/leaf) nodes, number of records,... .
     */
    void print_statistics() const;

    explicit operator nlohmann::json() const
    {
        nlohmann::json out;
        out["height"] = _height;
        out["root"] = node_to_json(_root);

        return out;
    }

protected:
    // Height of the tree.
    std::uint8_t _height = 1;

    // Isolation of tasks accessing a node.
    const mx::synchronization::isolation_level _isolation_level;

    // Select a preferred method for synchronization.
    const mx::synchronization::protocol _preferred_synchronization_protocol;

    // Pointer to the root.
    alignas(64) mx::resource::ptr _root;

    /**
     * Creates a new node.
     *
     * @param node_type Type of the node.
     * @param parent Parent of the node.
     * @param is_root True, if the new node will be the root.
     * @return Pointer to the new node.
     */
    [[nodiscard]] mx::resource::ptr create_node(const NodeType node_type, const mx::resource::ptr parent,
                                                const bool is_root) const
    {
        const auto is_inner = static_cast<bool>(node_type & NodeType::Inner);
        return mx::tasking::runtime::new_resource<Node<K, V>>(
            config::node_size(),
            mx::resource::hint{_isolation_level, _preferred_synchronization_protocol,
                               predict_access_frequency(is_inner, is_root), predict_read_write_ratio(is_inner)},
            node_type, parent);
    }

    /**
     * Creates a hint for tasking regarding usage of the node.
     *
     * @param is_inner True, of the node is an inner node.
     * @param is_root True, of the node is the root.
     * @return Hint for usage prediction which will be used for allocating resources.
     */
    [[nodiscard]] static mx::resource::hint::expected_access_frequency predict_access_frequency(const bool is_inner,
                                                                                                const bool is_root)
    {
        if (is_root)
        {
            return mx::resource::hint::expected_access_frequency::excessive;
        }

        if (is_inner)
        {
            return mx::resource::hint::expected_access_frequency::high;
        }

        return mx::resource::hint::expected_access_frequency::normal;
    }

    /**
     * Create a hint for the read/write ratio.
     * Inner nodes will be written very little while
     * leaf nodes will be written more often.
     *
     * @param is_inner True, when the node is an inner node.
     * @return  Predicted read/write ratio.
     */
    [[nodiscard]] static mx::resource::hint::expected_read_write_ratio predict_read_write_ratio(const bool is_inner)
    {
        return is_inner ? mx::resource::hint::expected_read_write_ratio::heavy_read
                        : mx::resource::hint::expected_read_write_ratio::balanced;
    }

    /**
     * Serializes a tree node to json format.
     *
     * @param node Node to serialize.
     * @return JSON representation of the node.
     */
    [[nodiscard]] nlohmann::json node_to_json(mx::resource::ptr node) const
    {
        auto out = nlohmann::json();
        auto node_ptr = mx::resource::ptr_cast<Node<K, V>>(node);

        out["channel_id"] = node.channel_id();
        out["is_leaf"] = node_ptr->is_leaf();
        out["size"] = node_ptr->size();

        if (node_ptr->is_inner())
        {
            auto children = nlohmann::json::array();
            for (auto i = 0U; i <= node_ptr->size(); ++i)
            {
                children.push_back(node_to_json(node_ptr->separator(i)));
            }
            out["children"] = children;
        }

        return out;
    }
};

template <typename K, typename V>
void BLinkTree<K, V>::create_new_root(const mx::resource::ptr left, const mx::resource::ptr right, const K key)
{
    const auto is_left_inner = mx::resource::ptr_cast<Node<K, V>>(left)->is_inner();
    mx::tasking::runtime::modify_predicted_usage(left, predict_access_frequency(is_left_inner, true),
                                                 predict_access_frequency(is_left_inner, false));

    auto root = this->create_inner_node(mx::resource::ptr_cast<Node<K, V>>(left)->is_leaf(), mx::resource::ptr{}, true);

    left.template get<Node<K, V>>()->parent(root);
    right.template get<Node<K, V>>()->parent(root);

    root.template get<Node<K, V>>()->separator(0, left);
    root.template get<Node<K, V>>()->insert(0, right, key);

    this->_height++;
    this->_root = root;
}

template <typename K, typename V>
std::pair<mx::resource::ptr, K> BLinkTree<K, V>::split(const mx::resource::ptr inner_node, const K key,
                                                       const mx::resource::ptr separator) const
{
    constexpr std::uint16_t left_size = InnerNode<K, V>::max_keys / 2;
    constexpr std::uint16_t right_size = InnerNode<K, V>::max_keys - left_size;

    auto node_ptr = mx::resource::ptr_cast<Node<K, V>>(inner_node);

    K key_up;
    auto new_inner_node = this->create_inner_node(node_ptr->is_branch(), node_ptr->parent());
    auto new_node_ptr = mx::resource::ptr_cast<Node<K, V>>(new_inner_node);

    new_node_ptr->high_key(node_ptr->high_key());

    if (key < node_ptr->inner_key(left_size - 1))
    {
        node_ptr->move(new_inner_node, left_size, right_size);
        new_node_ptr->separator(0, node_ptr->separator(left_size));
        new_node_ptr->size(right_size);
        node_ptr->size(left_size - 1);
        key_up = node_ptr->inner_key(left_size - 1);
        const auto index = node_ptr->index(key);
        separator.template get<Node<K, V>>()->parent(inner_node);
        node_ptr->insert(index, separator, key);
    }
    else if (key < node_ptr->inner_key(left_size))
    {
        node_ptr->move(new_inner_node, left_size, right_size);
        new_node_ptr->separator(0, separator);
        key_up = key;
        node_ptr->size(left_size);
        new_node_ptr->size(right_size);
    }
    else
    {
        node_ptr->move(new_inner_node, left_size + 1, right_size - 1);
        new_node_ptr->separator(0, node_ptr->separator(left_size + 1));
        node_ptr->size(left_size);
        new_node_ptr->size(right_size - 1);
        key_up = node_ptr->inner_key(left_size);

        const auto index = new_node_ptr->index(key);
        new_node_ptr->insert(index, separator, key);
    }

    new_node_ptr->right_sibling(node_ptr->right_sibling());
    node_ptr->right_sibling(new_inner_node);
    node_ptr->high_key(key_up);

    for (auto index = 0U; index <= new_node_ptr->size(); ++index)
    {
        new_node_ptr->separator(index).template get<Node<K, V>>()->parent(new_inner_node);
    }

    return {new_inner_node, key_up};
}

template <typename K, typename V>
std::pair<mx::resource::ptr, K> BLinkTree<K, V>::split(const mx::resource::ptr leaf_node_ptr, const K key,
                                                       const V value) const
{
    auto *leaf_node = mx::resource::ptr_cast<Node<K, V>>(leaf_node_ptr);

    constexpr std::uint16_t left_size = LeafNode<K, V>::max_items / 2;
    constexpr std::uint16_t right_size = LeafNode<K, V>::max_items - left_size;

    auto new_leaf_node_ptr = this->create_leaf_node(leaf_node->parent());
    auto *new_leaf_node = mx::resource::ptr_cast<Node<K, V>>(new_leaf_node_ptr);

    leaf_node->move(new_leaf_node_ptr, left_size, right_size);
    if (leaf_node->right_sibling() != nullptr)
    {
        new_leaf_node->right_sibling(leaf_node->right_sibling());
    }
    new_leaf_node->high_key(leaf_node->high_key());
    new_leaf_node->size(right_size);
    leaf_node->size(left_size);
    leaf_node->right_sibling(new_leaf_node_ptr);

    if (key < new_leaf_node->leaf_key(0))
    {
        leaf_node->insert(leaf_node->index(key), value, key);
    }
    else
    {
        new_leaf_node->insert(new_leaf_node->index(key), value, key);
    }

    leaf_node->high_key(new_leaf_node->leaf_key(0));

    return {new_leaf_node_ptr, new_leaf_node->leaf_key(0)};
}

template <typename K, typename V> void BLinkTree<K, V>::print_statistics() const
{
    NodeStatistics<K, V> statistics(this->height());

    for (auto node : *this)
    {
        statistics += node;
    }

    std::cout << statistics << std::endl;
}

template <typename K, typename V> void BLinkTree<K, V>::check() const
{
    for (auto node : *this)
    {
        NodeConsistencyChecker<K, V>::check_and_print_errors(node, std::cerr);
    }
}
} // namespace db::index::blinktree