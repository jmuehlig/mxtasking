#pragma once

#include "config.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <mx/synchronization/spinlock.h>
#include <mx/util/aligned_t.h>
#include <utility>
#include <vector>

namespace mx::memory::dynamic {

/**
 * Represents free space within an allocation block.
 * Holds the start and the size of a free object.
 */
class FreeHeader
{
public:
    constexpr FreeHeader(const std::uintptr_t start, const std::size_t size) noexcept : _start(start), _size(size) {}
    constexpr FreeHeader(const FreeHeader &other) noexcept = default;
    ~FreeHeader() noexcept = default;

    void contract(const std::size_t size) noexcept { _size -= size; }
    [[nodiscard]] std::uintptr_t start() const noexcept { return _start; }
    [[nodiscard]] std::uintptr_t size() const noexcept { return _size; }

    bool operator<(const FreeHeader &other) const noexcept { return _start < other._start; }
    bool operator>=(const std::size_t size) const noexcept { return _size >= size; }

    [[nodiscard]] bool borders(const FreeHeader &other) const noexcept { return (_start + _size) == other._start; }

    void merge(const FreeHeader &other) noexcept
    {
        if (other._start < _start)
        {
            assert(other.borders(*this) && "Can not merge: Elements are not next to each other");
            _start = other._start;
            _size += other._size;
        }
        else
        {
            assert(borders(other) && "Can not merge: Elements are not next to each other");
            _size += other._size;
        }
    }

private:
    std::uintptr_t _start;
    std::size_t _size;
};

/**
 * Header in front of allocated memory, storing the
 * size, the size which is unused because of alignment,
 * the ID of the NUMA node the memory is allocated on,
 * and the source allocation block of this memory.
 */
struct AllocatedHeader
{
    constexpr AllocatedHeader(const std::size_t size_, const std::uint16_t unused_size_before_header_,
                              const std::uint8_t numa_node_id_, const std::uint32_t allocation_block_id_) noexcept
        : size(size_), unused_size_before_header(unused_size_before_header_), numa_node_id(numa_node_id_),
          allocation_block_id(allocation_block_id_)
    {
    }

    const std::size_t size;
    const std::uint16_t unused_size_before_header;
    const std::uint8_t numa_node_id;
    const std::uint32_t allocation_block_id;
};

/**
 * Set of on or more free tiles, that can be allocated.
 */
class AllocationBlock
{
public:
    AllocationBlock(std::uint32_t id, std::uint8_t numa_node_id, std::size_t size);
    AllocationBlock(const AllocationBlock &other) = delete;
    AllocationBlock(AllocationBlock &&other) noexcept;
    AllocationBlock &operator=(AllocationBlock &&other) noexcept;
    ~AllocationBlock();

    /**
     * Allocates memory from the allocation block.
     *
     * @param alignment Requested alignment.
     * @param size Requested size.
     * @return Pointer to the allocated memory.
     */
    void *allocate(std::size_t alignment, std::size_t size) noexcept;

    /**
     * Frees memory.
     *
     * @param allocation_header Pointer to the header of the freed memory.
     */
    void free(AllocatedHeader *allocation_header) noexcept;

    /**
     * @return Unique number of this allocation block.
     */
    [[nodiscard]] std::uint32_t id() const noexcept { return _id; }

    /**
     * @return True, if the full block is free.
     */
    [[nodiscard]] bool is_free() const noexcept
    {
        return _free_elements.empty() || (_free_elements.size() == 1 && _free_elements[0].size() == _size);
    }

private:
    alignas(64) std::uint32_t _id;
    std::uint8_t _numa_node_id;
    std::size_t _size;

    void *_allocated_block;
    std::vector<FreeHeader> _free_elements;

    alignas(64) std::size_t _available_size;
    synchronization::Spinlock _lock;

    std::pair<std::vector<FreeHeader>::iterator, std::size_t> find_block(std::size_t alignment,
                                                                         std::size_t size) noexcept;
};

/**
 * Allocator which holds a set of allocation blocks separated
 * for each numa node region.
 */
class Allocator
{
public:
    Allocator();
    ~Allocator() = default;

    void *allocate(std::uint8_t numa_node_id, std::size_t alignment, std::size_t size) noexcept;
    void free(void *pointer) noexcept;

    /**
     * Frees unused allocation blocks.
     */
    void defragment() noexcept;

    /**
     * Releases all allocated memory.
     */
    void release_allocated_memory() noexcept;

    /**
     * Adds minimal memory to all numa node regions.
     */
    void initialize_empty();

    /**
     * @return True, if all blocks of all numa regions are free.
     */
    [[nodiscard]] bool is_free() const noexcept;

private:
    // Allocation blocks per numa node region.
    std::array<std::vector<AllocationBlock>, config::max_numa_nodes()> _numa_allocation_blocks;

    // Allocation flags, used for synchronization when allocating, per numa node region.
    std::array<util::aligned_t<std::atomic<bool>>, config::max_numa_nodes()> _numa_allocation_flags;

    // Sequence for block allocation per numa node region.
    std::array<util::aligned_t<std::atomic_uint32_t>, config::max_numa_nodes()> _next_allocation_id;

    /**
     * Allocates (thread-safe) a block of fresh memory
     * @param numa_node_id
     * @param size
     * @param blocks
     * @param flag
     */
    void allocate_new_block(std::uint8_t numa_node_id, std::size_t size, std::vector<AllocationBlock> &blocks,
                            std::atomic<bool> &flag);
};

} // namespace mx::memory::dynamic