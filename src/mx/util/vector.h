#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mx/memory/global_heap.h>
#include <utility>

namespace mx::util {
template <typename T, typename S = std::size_t> class vector
{
public:
    using value_type = T;
    using size_type = S;
    using reference_type = value_type &;
    using const_reference_type = value_type const &;
    using pointer_type = value_type *;
    using iterator = pointer_type;

    vector() : vector(size_type(16U)) {}

    vector(const size_type reserved) : vector(0U, reserved) {}

    vector(const std::uint8_t numa_node_id) : vector(numa_node_id, 16U) {}

    vector(const std::uint8_t numa_node_id, const size_type reserved) : _numa_node_id(numa_node_id)
    {
        reserve(reserved);
    }

    vector(const vector<T, S> &other)
        : _numa_node_id(other._numa_node_id), _current_index(other._current_index), _capacity(other._capacity)
    {
        _data = allocate(_numa_node_id, _capacity);
        std::memcpy(_data, other._data, sizeof(value_type) * _current_index);
    }

    vector(vector<T, S> &&other) noexcept
        : _numa_node_id(other._numa_node_id), _data(std::exchange(other._data, nullptr)),
          _current_index(std::exchange(other._current_index, 0U)), _capacity(std::exchange(other._capacity, 0U))
    {
    }

    ~vector()
    {
        auto *data = std::exchange(_data, nullptr);
        if (data != nullptr)
        {
            release(data, _capacity);
        }
    }

    vector<T, S> &operator=(const vector<T, S> &other)
    {
        if (_data != nullptr)
        {
            release(_data, _capacity);
        }

        _numa_node_id = other._numa_node_id;
        _current_index = other._current_index;
        _capacity = other._capacity;
        _data = allocate(_numa_node_id, _capacity);
        std::memcpy(_data, other._data, sizeof(value_type) * _current_index);
        return *this;
    }

    vector<T, S> &operator=(vector<T, S> &&other) noexcept
    {
        if (_data != nullptr)
        {
            release(_data, _capacity);
        }

        _numa_node_id = other._numa_node_id;
        _data = std::exchange(other._data, nullptr);
        _capacity = std::exchange(other._capacity, 0U);
        _current_index = std::exchange(other._current_index, 0U);
        return *this;
    }

    void reserve(const size_type n)
    {
        auto *old_data = std::exchange(_data, allocate(_numa_node_id, n));
        const auto old_capacity = std::exchange(_capacity, n);

        if (old_data != nullptr)
        {
            if (empty() == false)
            {
                std::memcpy(_data, old_data, sizeof(value_type) * _current_index);
            }

            release(old_data, old_capacity);
        }
    }

    void reserve(const std::uint8_t numa_node_id, const size_type n)
    {
        _numa_node_id = numa_node_id;
        reserve(n);
    }

    [[nodiscard]] size_type size() const noexcept { return _current_index; }

    [[nodiscard]] size_type capacity() const noexcept { return _capacity; }

    [[nodiscard]] bool empty() const noexcept { return _current_index == 0U; }

    void push_back(const_reference_type item)
    {
        grow_if_needed();
        at(_current_index++) = item;
    }

    void emplace_back(value_type &&item)
    {
        grow_if_needed();
        at(_current_index++) = std::move(item);
    }

    template <typename... Args> void emplace_back(Args &&... args)
    {
        grow_if_needed();
        new (&_data[_current_index++]) value_type(std::forward<Args>(args)...);
    }

    void clear() noexcept { _current_index = 0U; }

    [[nodiscard]] const_reference_type at(const size_type index) const noexcept { return _data[index]; }

    [[nodiscard]] reference_type at(const size_type index) noexcept { return _data[index]; }

    const_reference_type operator[](const size_type index) const noexcept { return at(index); }

    reference_type operator[](const size_type index) noexcept { return at(index); }

    [[nodiscard]] pointer_type data() noexcept { return _data; }

    [[nodiscard]] iterator begin() noexcept { return _data; }

    [[nodiscard]] iterator end() noexcept { return begin() + _current_index; }

    [[nodiscard]] iterator last() noexcept { return end() - 1U; }

private:
    std::uint8_t _numa_node_id{0U};
    pointer_type _data{nullptr};
    size_type _current_index{0U};
    size_type _capacity{0U};

    void grow_if_needed()
    {
        if (_current_index >= _capacity)
        {
            reserve(_capacity * 2U);
        }
    }

    static pointer_type allocate(const std::uint8_t numa_node_id, const std::size_t capacity) noexcept
    {
        const auto size = sizeof(value_type) * capacity;
        auto *data = static_cast<pointer_type>(memory::GlobalHeap::allocate(numa_node_id, size));
        assert(data != nullptr);
        return data;
    }

    static void release(pointer_type data, const std::size_t capacity) noexcept
    {
        const auto size = sizeof(value_type) * capacity;
        memory::GlobalHeap::free(static_cast<void *>(data), size);
    }
};
} // namespace mx::util