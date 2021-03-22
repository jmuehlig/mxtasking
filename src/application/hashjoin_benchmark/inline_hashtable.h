#pragma once

#include <cstdint>
#include <limits>
#include <mx/memory/alignment_helper.h>
#include <utility>

namespace application::hash_join {
/**
 * Hashtable for hashjoin.
 */
template <typename K, typename V> class InlineHashtable
{
private:
    struct Entry
    {
        constexpr Entry() noexcept : key(std::numeric_limits<K>::max()), value(0) {}
        Entry(Entry &&other) noexcept = default;
        ~Entry() noexcept = default;

        Entry &operator=(Entry &&) noexcept = default;
        K key;
        V value;
    };

public:
    static std::size_t needed_bytes(const std::size_t slots) noexcept
    {
        return sizeof(InlineHashtable<K, V>) +
               sizeof(InlineHashtable<K, V>::Entry) * mx::memory::alignment_helper::next_power_of_two(slots);
    }

    InlineHashtable(const std::size_t size)
        : _slots((size - sizeof(InlineHashtable<K, V>)) / sizeof(InlineHashtable<K, V>::Entry))
    {
        for (auto i = 0U; i < _slots; ++i)
        {
            at(i) = Entry{};
        }
    }
    ~InlineHashtable() = default;

    void insert(const K key, const V value) noexcept
    {
        for (auto index = hash(key);; ++index)
        {
            index &= _slots - 1U;

            auto &entry = at(index);
            if (entry.key != key && entry.key != std::numeric_limits<K>::max())
            {
                continue;
            }

            entry.key = key;
            entry.value = value;
            return;
        }
    }

    V get(const K key) const noexcept
    {
        for (auto index = hash(key);; ++index)
        {
            index &= _slots - 1U;

            const auto &entry = at(index);
            if (entry.key == key)
            {
                return entry.value;
            }

            if (entry.key == std::numeric_limits<K>::max())
            {
                return std::numeric_limits<V>::max();
            }
        }
    }

    const Entry &at(const std::size_t slot) const noexcept { return reinterpret_cast<const Entry *>(this + 1)[slot]; }

    Entry &at(const std::size_t slot) noexcept { return reinterpret_cast<Entry *>(this + 1)[slot]; }

private:
    const std::size_t _slots;

    std::size_t hash(K key) const
    {
        key ^= key >> 16;
        key *= 0x85ebca6b;
        key ^= key >> 13;
        key *= 0xc2b2ae35;
        key ^= key >> 16;
        return std::size_t(key);
    }
};
} // namespace application::hash_join