#pragma once
#include "config.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mx/resource/resource.h>
#include <mx/resource/resource_interface.h>
#include <mx/tasking/runtime.h>

namespace db::index::blinktree {

template <typename K, typename V> class Node;

/**
 * Node type.
 */
enum NodeType : std::uint8_t
{
    Leaf = 1U << 0U,
    Inner = 1U << 1U,
    Branch = 1U << 2U
};

inline NodeType operator|(const NodeType a, const NodeType b) noexcept
{
    return static_cast<NodeType>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/**
 * Header for every node
 */
template <typename K, typename V> struct NodeHeader
{
    static constexpr std::uint16_t node_size =
        config::node_size() - sizeof(NodeHeader<K, V>) - sizeof(mx::resource::ResourceInterface);

    // Type of the node.
    const NodeType node_type;

    // High key.
    K high_key{std::numeric_limits<K>::max()};

    // Link to the right sibling.
    mx::resource::ptr right_sibling;

    // Link to the parent. Alignment needed by some CPU architectures (e.g. arm) because of atomicity.
    alignas(8) std::atomic<mx::resource::ptr> parent;

    // Number of records in the node.
    std::uint16_t size{0U};

    [[maybe_unused]] NodeHeader(const NodeType node_type_, const mx::resource::ptr parent_) : node_type(node_type_)
    {
        this->parent.store(parent_);
    }

    ~NodeHeader() = default;
#ifdef __GNUG__
};
#else
} __attribute__((packed));
#endif

/**
 * Representation of an inner node.
 */
template <typename K, typename V> struct InnerNode
{
    static constexpr std::uint16_t max_keys =
        (NodeHeader<K, V>::node_size - sizeof(mx::resource::ptr)) / (sizeof(K) + sizeof(mx::resource::ptr));
    static constexpr std::uint16_t max_separators = max_keys + 1;

    // Memory for keys.
    std::array<K, InnerNode::max_keys> keys;

    // Memory for separators.
    std::array<mx::resource::ptr, InnerNode::max_separators> separators;
};

/**
 * Representation of a leaf node.
 */
template <typename K, typename V> struct LeafNode
{
    static constexpr std::uint16_t max_items = NodeHeader<K, V>::node_size / (sizeof(K) + sizeof(V));

    // Memory for keys.
    std::array<K, LeafNode::max_items> keys;

    // Memory for payloads.
    std::array<V, LeafNode::max_items> values;
};

/**
 * Abstract node representation.
 */
template <typename K, typename V> class Node final : public mx::resource::ResourceInterface
{
public:
    constexpr Node(const NodeType node_type, const mx::resource::ptr parent) : _header(node_type, parent)
    {
        static_assert(sizeof(Node<K, V>) <= config::node_size());
    }

    ~Node() override
    {
        if (is_inner())
        {
            for (auto i = 0U; i <= _header.size; ++i)
            {
                if (_inner_node.separators[i] != nullptr)
                {
                    mx::tasking::runtime::delete_resource<Node<K, V>>(_inner_node.separators[i]);
                }
            }
        }
    }

    void on_reclaim() override { this->~Node(); }

    /**
     * @return True, if this node is a leaf node.
     */
    [[nodiscard]] bool is_leaf() const noexcept { return _header.node_type & NodeType::Leaf; }

    /**
     * @return True, if this node is an inner node.
     */
    [[nodiscard]] bool is_inner() const noexcept { return _header.node_type & NodeType::Inner; }

    /**
     * @return True, if this node is an inner node and children are leaf nodes.
     */
    [[nodiscard]] bool is_branch() const noexcept { return _header.node_type & NodeType::Branch; }

    /**
     * @return Number of records stored in the node.
     */
    [[nodiscard]] std::uint16_t size() const noexcept { return _header.size; }

    /**
     * Updates the number of records stored in the node.
     * @param size New number of records.
     */
    void size(const std::uint16_t size) noexcept { _header.size = size; }

    /**
     * @return High key of the node.
     */
    K high_key() const noexcept { return _header.high_key; }

    /**
     * Updates the high key.
     * @param high_key New high key.
     */
    [[maybe_unused]] void high_key(const K high_key) noexcept { _header.high_key = high_key; }

    /**
     * @return Pointer to the right sibling.
     */
    [[nodiscard]] mx::resource::ptr right_sibling() const noexcept { return _header.right_sibling; }

    /**
     * Updates the right sibling.
     * @param right_sibling Pointer to the new right sibling.
     */
    [[maybe_unused]] void right_sibling(const mx::resource::ptr right_sibling) noexcept
    {
        _header.right_sibling = right_sibling;
    }

    /**
     * @return Pointer to the parent node.
     */
    [[nodiscard]] mx::resource::ptr parent() const noexcept { return _header.parent; }

    /**
     * Updates the parent node.
     * @param parent Pointer to the new parent node.
     */
    void parent(const mx::resource::ptr parent) noexcept { _header.parent = parent; }

    /**
     * Read the value at a given index.
     * @param index Index.
     * @return Value at the index.
     */
    V value(const std::uint16_t index) const noexcept { return _leaf_node.values[index]; }

    /**
     * Update the value at a given index.
     * @param index Index.
     * @param value New value.
     */
    void value(const std::uint16_t index, const V value) noexcept { _leaf_node.values[index] = value; }

    /**
     * Read the separator at a given index.
     * @param index Index.
     * @return Separator at the index.
     */
    [[nodiscard]] mx::resource::ptr separator(const std::uint16_t index) const noexcept
    {
        return _inner_node.separators[index];
    }

    /**
     * Update the separator for a given index.
     * @param index Index.
     * @param separator New separator for the index.
     */
    void separator(const std::uint16_t index, const mx::resource::ptr separator) noexcept
    {
        _inner_node.separators[index] = separator;
    }

    /**
     * Read the key from the leaf node.
     * @param index Index.
     * @return Key at the index.
     */
    K leaf_key(const std::uint16_t index) const noexcept { return _leaf_node.keys[index]; }

    /**
     * Read the key from the inner node.
     * @param index Index.
     * @return Key at the index.
     */
    K inner_key(const std::uint16_t index) const noexcept { return _inner_node.keys[index]; }

    /**
     * @return True, if the node can not store further records.
     */
    [[nodiscard]] bool full() const noexcept
    {
        const auto max_size = is_leaf() ? LeafNode<K, V>::max_items : InnerNode<K, V>::max_keys;
        return _header.size >= max_size;
    }

    /**
     * Calculates the index for a given key.
     * @param key Key.
     * @return Index for the key.
     */
    std::uint16_t index(K key) const noexcept;

    /**
     * Calculates the child for a given key using binary search.
     * @param key Key.
     * @return Child for the key.
     */
    mx::resource::ptr child(K key) const noexcept;

    /**
     * Inserts a record into an inner node.
     * @param index Index.
     * @param separator Separator.
     * @param key Key.
     */
    void insert(std::uint16_t index, mx::resource::ptr separator, K key);

    /**
     * Inserts a record into a leaf node.
     * @param index Index.
     * @param value Payload.
     * @param key Key.
     */
    void insert(std::uint16_t index, V value, K key);

    /**
     * Moves a range of records into another node.
     * @param destination Other node.
     * @param from_index Start index.
     * @param count Number of records to move.
     */
    void move(mx::resource::ptr destination, std::uint16_t from_index, std::uint16_t count);

    /**
     * Searches a separator within an inner node.
     * @param separator Separator to search.
     * @return True, if the separator was found.
     */
    [[nodiscard]] bool contains(mx::resource::ptr separator) const noexcept;

private:
    NodeHeader<K, V> _header;
    union {
        InnerNode<K, V> _inner_node;
        LeafNode<K, V> _leaf_node;
    };
};

template <typename K, typename V> std::uint16_t Node<K, V>::index(const K key) const noexcept
{
    const auto keys = this->is_leaf() ? this->_leaf_node.keys.cbegin() : this->_inner_node.keys.cbegin();
    const auto iterator = std::lower_bound(keys, keys + this->size(), key);

    return std::distance(keys, iterator);
}

template <typename K, typename V> mx::resource::ptr Node<K, V>::child(const K key) const noexcept
{
    std::int16_t low = 0;
    std::int16_t high = size() - 1;
    while (low <= high)
    {
        const auto mid = (low + high) >> 1U; // Will work for size() - 1 < max(std::int32_t)/2
        if (this->inner_key(mid) <= key)
        {
            low = mid + 1;
        }
        else
        {
            high = mid - 1;
        }
    }

    return this->_inner_node.separators[high + 1U];
}

template <typename K, typename V>
void Node<K, V>::insert(const std::uint16_t index, const mx::resource::ptr separator, const K key)
{
    if (index < this->size())
    {
        const auto offset = this->size() - index;
        std::memmove(static_cast<void *>(&this->_inner_node.keys[index + 1]),
                     static_cast<void *>(&this->_inner_node.keys[index]), offset * sizeof(K));
        std::memmove(static_cast<void *>(&this->_inner_node.separators[index + 2]),
                     static_cast<void *>(&this->_inner_node.separators[index + 1]), offset * sizeof(mx::resource::ptr));
    }

    this->_inner_node.keys[index] = key;
    this->_inner_node.separators[index + 1] = separator;
    ++this->_header.size;
}

template <typename K, typename V> void Node<K, V>::insert(const std::uint16_t index, const V value, const K key)
{
    if (index < this->size())
    {
        const auto offset = this->size() - index;
        std::memmove(static_cast<void *>(&this->_leaf_node.keys[index + 1]),
                     static_cast<void *>(&this->_leaf_node.keys[index]), offset * sizeof(K));
        std::memmove(static_cast<void *>(&this->_leaf_node.values[index + 1]),
                     static_cast<void *>(&this->_leaf_node.values[index]), offset * sizeof(V));
    }

    this->_leaf_node.keys[index] = key;
    this->_leaf_node.values[index] = value;
    ++this->_header.size;
}

template <typename K, typename V>
void Node<K, V>::move(const mx::resource::ptr destination, const std::uint16_t from_index, const std::uint16_t count)
{
    auto *node = mx::resource::ptr_cast<Node<K, V>>(destination);
    if (this->is_leaf())
    {
        std::memcpy(static_cast<void *>(&node->_leaf_node.keys[0]),
                    static_cast<void *>(&this->_leaf_node.keys[from_index]), count * sizeof(K));
        std::memcpy(static_cast<void *>(&node->_leaf_node.values[0]),
                    static_cast<void *>(&this->_leaf_node.values[from_index]), count * sizeof(V));
    }
    else
    {
        std::memcpy(static_cast<void *>(&node->_inner_node.keys[0]),
                    static_cast<void *>(&this->_inner_node.keys[from_index]), count * sizeof(K));
        std::memcpy(static_cast<void *>(&node->_inner_node.separators[1]),
                    static_cast<void *>(&this->_inner_node.separators[from_index + 1]),
                    count * sizeof(mx::resource::ptr));
    }
}

template <typename K, typename V> bool Node<K, V>::contains(const mx::resource::ptr separator) const noexcept
{
    for (auto i = 0U; i <= this->size(); ++i)
    {
        if (this->_inner_node.separators[i] == separator)
        {
            return true;
        }
    }

    return false;
}
} // namespace db::index::blinktree