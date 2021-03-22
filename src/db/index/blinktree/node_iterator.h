#pragma once

#include "node.h"
#include <mx/resource/resource.h>

namespace db::index::blinktree {
/**
 * Iterator for iterating over nodes of a tree.
 */
template <typename K, typename V> class NodeIterator
{
public:
    NodeIterator() = default;
    explicit NodeIterator(Node<K, V> *root) : _current_node(root), _first_node_in_level(root) {}
    ~NodeIterator() = default;

    Node<K, V> *&operator*() { return _current_node; }

    NodeIterator<K, V> &operator++()
    {
        if (_current_node->right_sibling())
        {
            _current_node = _current_node->right_sibling().template get<Node<K, V>>();
        }
        else if (_current_node->is_inner())
        {
            _first_node_in_level = _first_node_in_level->separator(0).template get<Node<K, V>>();
            _current_node = _first_node_in_level;
        }
        else
        {
            _current_node = nullptr;
        }

        return *this;
    }

    bool operator!=(const NodeIterator<K, V> &other) const { return _current_node != other._current_node; }

private:
    Node<K, V> *_current_node = nullptr;
    Node<K, V> *_first_node_in_level = nullptr;
};
} // namespace db::index::blinktree