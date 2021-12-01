#pragma once
#include <cassert>
#include <cstdint>
#include <functional>

namespace mx::memory {
/**
 * Holds the memory address of an instance of the class T
 * and decodes a 16bit core address within the memory address.
 * The size of the tagged_ptr<T> is equal to T*.
 */
template <class T, typename I> class tagged_ptr
{
public:
    constexpr tagged_ptr() noexcept
    {
        static_assert(sizeof(I) == 2U);
        static_assert(sizeof(tagged_ptr) == 8U);
    }

    constexpr explicit tagged_ptr(T *pointer) noexcept : _object_pointer(std::uintptr_t(pointer)) {}

    constexpr explicit tagged_ptr(T *pointer, const I information) noexcept
        : _object_pointer(std::uintptr_t(pointer)), _information(information)
    {
    }

    ~tagged_ptr() noexcept = default;

    /**
     * @return The decoded info.
     */
    inline I info() const noexcept { return _information; }

    /**
     * @return The memory address without the info.
     */
    template <typename S = T> inline S *get() const noexcept { return reinterpret_cast<S *>(_object_pointer); }

    /**
     * Decodes the given info within the pointer.
     *
     * @param info  Info to store in the tagged pointer.
     */
    inline void reset(const I information) noexcept { _information = information; }

    /**
     * Replaces the internal pointer by a new one.
     *
     * @param new_pointer Pointer to the new memory object.
     */
    inline void reset(T *new_pointer = nullptr) noexcept { _object_pointer = std::uintptr_t(new_pointer); }

    T *operator->() const noexcept { return get(); }

    explicit operator T *() const noexcept { return get(); }

    explicit operator bool() const noexcept { return _object_pointer > 0U; }

    tagged_ptr<T, I> &operator=(const tagged_ptr<T, I> &other) noexcept = default;

    bool operator==(const tagged_ptr<T, I> &other) const noexcept { return other._object_pointer == _object_pointer; }

    bool operator==(const T *other) const noexcept { return other == get(); }

    bool operator==(std::nullptr_t) const noexcept { return _object_pointer == 0U; }

    bool operator!=(const tagged_ptr<T, I> &other) const noexcept { return other.get() != get(); }

    bool operator!=(std::nullptr_t) const noexcept { return _object_pointer != 0U; }

    bool operator<(const tagged_ptr<T, I> &other) noexcept { return other.get() < get(); }

    bool operator<=(const tagged_ptr<T, I> &other) noexcept { return other.get() <= get(); }

    bool operator>(const tagged_ptr<T, I> &other) noexcept { return other.get() > get(); }

    bool operator>=(const tagged_ptr<T, I> &other) noexcept { return other.get() >= get(); }

private:
    /**
     * Pointer to the instance of T, only 48bit are used.
     */
    std::uintptr_t _object_pointer : 48 {0U};

    /**
     * Information stored within this pointer, remaining 16bit are used.
     */
    I _information{};
} __attribute__((packed));
} // namespace mx::memory

namespace std {
template <class T, typename I> struct hash<mx::memory::tagged_ptr<T, I>>
{
    std::size_t operator()(const mx::memory::tagged_ptr<T, I> &ptr) const noexcept
    {
        return std::hash<T *>().operator()(ptr.get());
    }
};
} // namespace std