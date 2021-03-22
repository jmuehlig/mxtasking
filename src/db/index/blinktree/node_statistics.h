#pragma once

#include "config.h"
#include "node.h"
#include <cstdint>
#include <ostream>

namespace db::index::blinktree {
/**
 * Collects and prints statistics of a set of nodes.
 */
template <typename K, typename V> class NodeStatistics
{
public:
    explicit NodeStatistics(const std::uint16_t height) : _tree_height(height) {}
    ~NodeStatistics() = default;

    NodeStatistics &operator+=(Node<K, V> *node)
    {
        this->_count_inner_nodes += node->is_inner();
        this->_count_leaf_nodes += node->is_leaf();

        if (node->is_leaf())
        {
            this->_count_leaf_node_keys += node->size();
        }
        else
        {
            this->_count_inner_node_keys += node->size();
        }

        return *this;
    }

    friend std::ostream &operator<<(std::ostream &stream, const NodeStatistics<K, V> &tree_statistics)
    {
        const auto count_nodes = tree_statistics._count_leaf_nodes + tree_statistics._count_inner_nodes;
        const auto size_in_bytes = count_nodes * config::node_size();
        stream << "Statistics of the Tree: \n"
               << "  Node   size:    " << sizeof(Node<K, V>) << " B\n"
               << "  Header size:    " << sizeof(NodeHeader<K, V>) << " B\n"
               << "  Inner  keys:    " << InnerNode<K, V>::max_keys << " (" << sizeof(K) * InnerNode<K, V>::max_keys
               << " B)\n"
               << "  Leaf   keys:    " << LeafNode<K, V>::max_items << " (" << sizeof(K) * LeafNode<K, V>::max_items
               << " B)\n"
               << "  Tree   height:  " << tree_statistics._tree_height << "\n"
               << "  Inner  nodes:   " << tree_statistics._count_inner_nodes << "\n"
               << "  Inner  entries: " << tree_statistics._count_inner_node_keys << "\n"
               << "  Leaf   nodes:   " << tree_statistics._count_leaf_nodes << "\n"
               << "  Leaf   entries: " << tree_statistics._count_leaf_node_keys << "\n"
               << "  Tree   size:    " << size_in_bytes / 1024.0 / 1024.0 << " MB";

        return stream;
    }

private:
    // Number of inner nodes.
    std::uint64_t _count_inner_nodes = 0U;

    // Number of leaf nodes.
    std::uint64_t _count_leaf_nodes = 0U;

    // Number of records located in inner nodes.
    std::uint64_t _count_inner_node_keys = 0U;

    // Number of records located in leaf nodes.
    std::uint64_t _count_leaf_node_keys = 0U;

    // Hight of the tree.
    const std::uint16_t _tree_height;
};
} // namespace db::index::blinktree