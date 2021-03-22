#pragma once

namespace db::index::blinktree {
template <typename K, typename V> class Listener
{
public:
    virtual void inserted(std::uint16_t core_id, K key, V value) = 0;
    virtual void updated(std::uint16_t core_id, K key, V value) = 0;
    virtual void removed(std::uint16_t core_id, K key) = 0;
    virtual void found(std::uint16_t core_id, K key, V value) = 0;
    virtual void missing(std::uint16_t core_id, K key) = 0;
};
} // namespace db::index::blinktree