#pragma once
#include <ostream>

#include "node.h"

namespace db::index::blinktree {
/**
 * Validates tree nodes and checks consistency.
 */
template <typename K, typename V> class NodeConsistencyChecker
{
public:
    /**
     * Validates the node and prints errors to the given stream.
     * @param node Node to validate.
     * @param stream Stream to print errors.
     */
    static void check_and_print_errors(Node<K, V> *node, std::ostream &stream);

private:
    static void check_high_key_valid(Node<K, V> *node, std::ostream &stream);
    static void check_key_order_valid(Node<K, V> *node, std::ostream &stream);
    static void check_no_null_separator(Node<K, V> *node, std::ostream &stream);
    static void check_children_order_valid(Node<K, V> *node, std::ostream &stream);
    static void check_level_valid(Node<K, V> *node, std::ostream &stream);
    static void check_and_print_parent(Node<K, V> *node, std::ostream &stream);
};

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_and_print_errors(Node<K, V> *node, std::ostream &stream)
{
    check_high_key_valid(node, stream);
    check_key_order_valid(node, stream);
    check_no_null_separator(node, stream);
    check_children_order_valid(node, stream);
    check_level_valid(node, stream);

    // check_and_print_parent(node, stream);
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_high_key_valid(Node<K, V> *node, std::ostream &stream)
{
    if (node->is_leaf())
    {
        if (node->leaf_key(node->size() - 1) >= node->high_key())
        {
            stream << "[HighKey   ] Leaf " << node << ": Key[" << node->size() - 1
                   << "] (=" << node->leaf_key(node->size() - 1) << ") >= " << node->high_key() << std::endl;
        }
    }
    else
    {
        if (node->inner_key(node->size() - 1) >= node->high_key())
        {
            stream << "[HighKey   ] Inner " << node << ": Key[" << node->size() - 1
                   << "] (=" << node->inner_key(node->size() - 1) << ") >= " << node->high_key() << std::endl;
        }
    }
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_key_order_valid(Node<K, V> *node, std::ostream &stream)
{
    for (auto index = 1U; index < node->size(); index++)
    {
        if (node->is_leaf())
        {
            if (node->leaf_key(index - 1U) >= node->leaf_key(index))
            {
                stream << "[KeyOrder  ] Leaf " << node << ": Key[" << index - 1U << "] (=" << node->leaf_key(index - 1U)
                       << ") >= Key[" << index << "] (=" << node->leaf_key(index) << ")" << std::endl;
            }
        }
        else
        {
            if (node->inner_key(index - 1) >= node->inner_key(index))
            {
                stream << "[KeyOrder  ] Inner " << node << ": Key[" << index - 1 << "] (=" << node->inner_key(index - 1)
                       << ") >= Key[" << index << "] (=" << node->inner_key(index) << ")" << std::endl;
            }
        }
    }
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_no_null_separator(Node<K, V> *node, std::ostream &stream)
{
    if (node->is_inner())
    {
        for (auto index = 0U; index <= node->size(); index++)
        {
            if (node->separator(index) == nullptr)
            {
                stream << "[Separator ] Inner " << node << ": Separator[" << index << "] is empty." << std::endl;
            }
        }
    }
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_children_order_valid(Node<K, V> *node, std::ostream &stream)
{
    if (node->is_inner())
    {
        for (auto index = 0U; index < node->size(); index++)
        {
            auto child = node->separator(index).template get<Node<K, V>>();
            const auto child_last_key =
                child->is_leaf() ? child->leaf_key(child->size() - 1U) : child->inner_key(child->size() - 1U);
            if (child_last_key >= node->inner_key(index))
            {
                stream << "[ChildOrder] Inner " << node << ": Key[" << index << "] (=" << node->inner_key(index)
                       << ") <= Separator[" << index << "].Key[" << child->size() - 1U << "] (=" << child_last_key
                       << ")" << std::endl;
            }
        }
    }
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_level_valid(Node<K, V> *node, std::ostream &stream)
{
    if (node->right_sibling() && node->is_leaf() != node->right_sibling().template get<Node<K, V>>()->is_leaf())
    {
        stream << "[Level     ] Leaf " << node << ": Is marked as leaf, but right sibling is not" << std::endl;
    }

    if (node->is_inner())
    {
        for (auto index = 0U; index < node->size(); index++)
        {
            if (node->separator(index).template get<Node<K, V>>()->is_leaf() !=
                node->separator(index + 1U).template get<Node<K, V>>()->is_leaf())
            {
                stream << "[Level     ] Inner " << node << ": Separator[" << index
                       << "] is marked as is_leaf = " << node->separator(index).template get<Node<K, V>>()->is_leaf()
                       << " but Separator[" << index + 1U << "] is not" << std::endl;
            }
        }
    }
}

template <typename K, typename V>
void NodeConsistencyChecker<K, V>::check_and_print_parent(Node<K, V> *node, std::ostream &stream)
{
    const auto parent = node->parent();
    if (parent)
    {
        if (parent.template get<Node<K, V>>()->contains(mx::resource::ptr(node)) == false)
        {
            stream << "Wrong parent(1) for node " << node << " (leaf: " << node->is_leaf() << ")" << std::endl;
        }
        else
        {
            auto index = 0U;
            for (; index <= parent.template get<Node<K, V>>()->size(); index++)
            {
                if (parent.template get<Node<K, V>>()->separator(index).template get<Node<K, V>>() == node)
                {
                    break;
                }
            }

            if (index < parent.template get<Node<K, V>>()->size())
            {
                const auto key =
                    node->is_leaf() ? node->leaf_key(node->size() - 1U) : node->inner_key(node->size() - 1);
                if ((key < parent.template get<Node<K, V>>()->inner_key(index)) == false)
                {
                    stream << "Wrong parent(2) for node " << node << " (leaf: " << node->is_leaf() << ")" << std::endl;
                }
            }
            else
            {
                const auto key = node->is_leaf() ? node->leaf_key(0U) : node->inner_key(0U);
                if ((key >= parent.template get<Node<K, V>>()->inner_key(index - 1U)) == false)
                {
                    stream << "Wrong parent(3) for node " << node << " (leaf: " << node->is_leaf() << ")" << std::endl;
                }
            }
        }
    }
}
} // namespace db::index::blinktree
