#pragma once
#include <type_traits>

namespace mx::util {
/**
 * Aligns the given data type with an alignment of 64.
 */
template <typename T> class alignas(64) aligned_t
{
public:
    constexpr aligned_t() noexcept = default;

    explicit constexpr aligned_t(const T &value) noexcept : _value(value) {}
    constexpr aligned_t(const aligned_t<T> &other) = default;

    template <typename... Args> explicit aligned_t(Args &&... args) noexcept : _value(std::forward<Args>(args)...) {}

    ~aligned_t() noexcept = default;

    aligned_t<T> &operator=(const aligned_t<T> &) = default;
    aligned_t<T> &operator=(aligned_t<T> &&) noexcept = default;

    explicit operator T() const noexcept { return _value; }

    T &operator*() noexcept { return _value; };
    const T &operator*() const noexcept { return _value; };

    T &value() noexcept { return _value; }
    const T &value() const noexcept { return _value; }

    aligned_t<T> &operator=(const T &value) noexcept
    {
        _value = value;
        return *this;
    }

    bool operator==(std::nullptr_t) const noexcept
    {
        if constexpr (std::is_pointer<T>::value)
        {
            return _value == nullptr;
        }
        else
        {
            return false;
        }
    }

    bool operator!=(std::nullptr_t) const noexcept
    {
        if constexpr (std::is_pointer<T>::value)
        {
            return _value != nullptr;
        }
        else
        {
            return true;
        }
    }

private:
    T _value = T();
};
} // namespace mx::util