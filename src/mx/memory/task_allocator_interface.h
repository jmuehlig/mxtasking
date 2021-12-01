#pragma once

#include <cstdint>
#include <cstdlib>

namespace mx::memory {
/**
 * Interface for task allocators (e.g. using systems malloc
 * or the internal allocator).
 */
class TaskAllocatorInterface
{
public:
    constexpr TaskAllocatorInterface() noexcept = default;
    virtual ~TaskAllocatorInterface() noexcept = default;

    /**
     * Allocates memory for the given core.
     * @param core_id Core to allocate memory for.
     * @return Allocated memory.
     */
    [[nodiscard]] virtual void *allocate(std::uint16_t core_id) = 0;

    /**
     * Frees the memory at the given core.
     * @param core_id Core to store free memory.
     * @param address Address to free.
     */
    virtual void free(std::uint16_t core_id, void *address) noexcept = 0;
};

/**
 * Task allocator using the systems (aligned_)malloc/free interface.
 */
template <std::size_t S> class SystemTaskAllocator final : public TaskAllocatorInterface
{
public:
    constexpr SystemTaskAllocator() noexcept = default;
    ~SystemTaskAllocator() noexcept override = default;

    /**
     * @return Allocated memory using systems malloc (but aligned).
     */
    [[nodiscard]] void *allocate(const std::uint16_t /*core_id*/) override { return std::aligned_alloc(64U, S); }

    /**
     * Frees the given memory using systems free.
     * @param address Memory to free.
     */
    void free(const std::uint16_t /*core_id*/, void *address) noexcept override { std::free(address); }
};
} // namespace mx::memory