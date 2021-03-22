#pragma once
#include "alignment_helper.h"
#include <cstdint>
#include <cstdlib>
#include <numa.h>

namespace mx::memory {
/**
 * The global heap represents the heap, provided by the OS.
 */
class GlobalHeap
{
public:
    /**
     * Allocates the given size on the given NUMA node.
     *
     * @param numa_node_id ID of the NUMA node, the memory should allocated on.
     * @param size  Size of the memory to be allocated.
     * @return Pointer to allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        return numa_alloc_onnode(size, numa_node_id);
    }

    /**
     * Allocates the given memory aligned to the cache line
     * with a multiple of the alignment as a size.
     * The allocated memory is not NUMA aware.
     * @param size Size to be allocated.
     * @return Allocated memory
     */
    static void *allocate_cache_line_aligned(const std::size_t size)
    {
        return std::aligned_alloc(64U, alignment_helper::next_multiple(size, 64UL));
    }

    /**
     * Frees the given memory.
     *
     * @param memory Pointer to memory.
     * @param size Size of the allocated object.
     */
    static void free(void *memory, const std::size_t size) { numa_free(memory, size); }
};
} // namespace mx::memory