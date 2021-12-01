#include "dynamic_size_allocator.h"
#include "global_heap.h"
#include <algorithm>
#include <cassert>
#include <mx/system/topology.h>

using namespace mx::memory::dynamic;

AllocationBlock::AllocationBlock(const std::uint32_t id, const std::uint8_t numa_node_id, const std::size_t size)
    : _id(id), _numa_node_id(numa_node_id), _size(size), _available_size(size)
{
    this->_allocated_block = GlobalHeap::allocate(numa_node_id, size);
    this->_free_elements.emplace_back(FreeHeader{reinterpret_cast<std::uintptr_t>(this->_allocated_block), size});
}

AllocationBlock::AllocationBlock(AllocationBlock &&other) noexcept
    : _id(other._id), _numa_node_id(other._numa_node_id), _size(other._size),
      _allocated_block(std::exchange(other._allocated_block, nullptr)), _free_elements(std::move(other._free_elements)),
      _available_size(other._available_size)
{
}

AllocationBlock &AllocationBlock::operator=(AllocationBlock &&other) noexcept
{
    this->_id = other._id;
    this->_numa_node_id = other._numa_node_id;
    this->_size = other._size;
    this->_allocated_block = std::exchange(other._allocated_block, nullptr);
    this->_free_elements = std::move(other._free_elements);
    this->_available_size = other._available_size;
    return *this;
}

AllocationBlock::~AllocationBlock()
{
    if (this->_allocated_block != nullptr)
    {
        GlobalHeap::free(this->_allocated_block, this->_size);
    }
}

void *AllocationBlock::allocate(const std::size_t alignment, const std::size_t size) noexcept
{
    assert(alignment && (!(alignment & (alignment - 1))) && "Alignment must be > 0 and power of 2");
    this->_lock.lock();

    if (this->_available_size < size)
    {
        this->_lock.unlock();
        return nullptr;
    }

    auto [free_element_iterator, aligned_size_including_header] = this->find_block(alignment, size);
    if (free_element_iterator == this->_free_elements.end())
    {
        this->_lock.unlock();
        return nullptr;
    }

    const auto free_block_start = free_element_iterator->start();
    const auto free_block_end = free_block_start + free_element_iterator->size();
    const auto remaining_size = free_element_iterator->size() - aligned_size_including_header;

    std::uint16_t size_before_header{0U};
    if (remaining_size >= 256U)
    {
        const auto index = std::distance(this->_free_elements.begin(), free_element_iterator);
        this->_free_elements[index].contract(aligned_size_including_header);
        this->_available_size -= aligned_size_including_header;
    }
    else
    {
        size_before_header = remaining_size;
        this->_free_elements.erase(free_element_iterator);
        this->_available_size -= free_element_iterator->size();
    }
    this->_lock.unlock();

    const auto allocation_header_address = free_block_end - aligned_size_including_header;
    new (reinterpret_cast<void *>(allocation_header_address)) AllocatedHeader(
        aligned_size_including_header - sizeof(AllocatedHeader), size_before_header, this->_numa_node_id, this->_id);
    assert((allocation_header_address + sizeof(AllocatedHeader)) % alignment == 0 && "Not aligned");

    return reinterpret_cast<void *>(allocation_header_address + sizeof(AllocatedHeader));
}

void AllocationBlock::free(AllocatedHeader *allocation_header) noexcept
{
    const auto allocated_size = allocation_header->size;
    const auto unused_size_before_header = allocation_header->unused_size_before_header;
    const auto block_address = reinterpret_cast<std::uintptr_t>(allocation_header) - unused_size_before_header;
    const auto size = allocated_size + unused_size_before_header + sizeof(AllocatedHeader);

    const auto free_element = FreeHeader{block_address, size};

    this->_lock.lock();

    if (this->_free_elements.empty())
    {
        this->_free_elements.push_back(free_element);
    }
    else
    {
        const auto lower_bound_iterator =
            std::lower_bound(this->_free_elements.begin(), this->_free_elements.end(), free_element);
        const auto index = std::distance(this->_free_elements.begin(), lower_bound_iterator);
        assert(index >= 0 && "Index is negative");
        const auto real_index = std::size_t(index);

        // Try merge to the right.
        if (real_index < this->_free_elements.size() && free_element.borders(this->_free_elements[real_index]))
        {
            this->_free_elements[real_index].merge(free_element);

            // Okay, we inserted the new free element as merge,  we do not insert it "real".
            // Try to merge the expanded right with the left.
            if (real_index > 0U && this->_free_elements[real_index - 1U].borders(this->_free_elements[real_index]))
            {
                this->_free_elements[real_index - 1].merge(this->_free_elements[real_index]);
                this->_free_elements.erase(this->_free_elements.begin() + real_index);
            }
        }
        else if (real_index > 0U && this->_free_elements[real_index - 1U].borders(free_element))
        {
            // In this case, we could not merge with the right, but can we merge
            // to the left? By this, we could save up the real insert.
            this->_free_elements[real_index - 1U].merge(free_element);
        }
        else
        {
            // We could not merge anything. Just insert.
            this->_free_elements.insert(this->_free_elements.begin() + real_index, free_element);
        }
    }
    this->_available_size += free_element.size();

    this->_lock.unlock();
}

std::pair<std::vector<FreeHeader>::iterator, std::size_t> AllocationBlock::find_block(const std::size_t alignment,
                                                                                      const std::size_t size) noexcept
{
    /**
     * Check each block of the free list for enough space to include the wanted space.
     * If enough, check the alignment (starting at the end).
     *
     * +----------------------------+
     * | 2000byte                   |
     * +----------------------------+
     *  => wanted: 700byte
     *  => align border -> 1300 is not aligned, expand to 720byte -> 1280 is aligned
     * +----------------------------+
     * | 1280byte       | 720byte   |
     * +----------------------------+
     *
     */

    const auto size_including_header = size + sizeof(AllocatedHeader);

    for (auto iterator = this->_free_elements.begin(); iterator != this->_free_elements.end(); iterator++)
    {
        const auto &free_element = *iterator;
        if (free_element >= size_including_header)
        {
            const auto start = free_element.start();

            // The free block ends here.
            const auto end = start + free_element.size();

            // This is where we would start the memory block on allocation
            // But this may be not aligned.
            const auto possible_block_begin = end - size;

            // This is the size we need to start the block aligned.
            const auto aligned_size = size + (possible_block_begin & (alignment - 1U));

            // This is the size we need aligned and for header.
            const auto aligned_size_including_header = aligned_size + sizeof(AllocatedHeader);

            if (free_element >= aligned_size_including_header)
            {
                // aligned_size_including_header
                return std::make_pair(iterator, aligned_size_including_header);
            }
        }
    }

    return std::make_pair(this->_free_elements.end(), 0U);
}

Allocator::Allocator()
{
    this->initialize_empty();
}

void *Allocator::allocate(const std::uint8_t numa_node_id, const std::size_t alignment, const std::size_t size) noexcept
{
    auto &allocation_blocks = this->_numa_allocation_blocks[numa_node_id];

    auto *memory = allocation_blocks.back().allocate(alignment, size);
    if (memory == nullptr)
    {
        // This will be allocated default...
        constexpr auto default_alloc_size = 1UL << 28U;

        // ... but if the requested size is higher, allocate more.
        const auto size_to_alloc = std::max(default_alloc_size, alignment_helper::next_multiple(size, 64UL));

        // Try to allocate until allocation was successful.
        // It is possible, that another core tries to allocate at the
        // same time, therefore we capture the allocation flag (one per region)
        auto &flag = this->_numa_allocation_flags[numa_node_id].value();
        while (memory == nullptr)
        {
            allocate_new_block(numa_node_id, size_to_alloc, allocation_blocks, flag);
            memory = allocation_blocks.back().allocate(alignment, size);
        }
    }

    return memory;
}

void Allocator::allocate_new_block(const std::uint8_t numa_node_id, const std::size_t size,
                                   std::vector<AllocationBlock> &blocks, std::atomic<bool> &flag)
{
    // Acquire the allocation flag to ensure only one thread to allocate.
    auto expected = false;
    const auto can_allocate = flag.compare_exchange_strong(expected, true);

    if (can_allocate)
    {
        // If that was this thread go for it...
        const auto next_id = this->_next_allocation_id[numa_node_id].value().fetch_add(1U, std::memory_order_acq_rel);
        blocks.emplace_back(AllocationBlock{next_id, numa_node_id, size});

        // .. but release the allocation flag afterward.
        flag.store(false);
    }
    else
    {
        // If that was another thread, wait until he finished.
        while (flag.load())
        {
            system::builtin::pause();
        }
    }
}

void Allocator::free(void *pointer) noexcept
{
    // Every allocated memory belongs to one allocation block.
    // The reason is, that we can only return full blocks to
    // the global heap that is managed by the operating system.
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);

    // Access the header to identify the allocation block.
    const auto header_address = address - sizeof(AllocatedHeader);
    auto *allocation_header = reinterpret_cast<AllocatedHeader *>(header_address);

    // Check all blocks to find the matching one.
    for (auto &block : this->_numa_allocation_blocks[allocation_header->numa_node_id])
    {
        if (allocation_header->allocation_block_id == block.id())
        {
            block.free(allocation_header);
            return;
        }
    }
}

void Allocator::defragment() noexcept
{
    // Remove all blocks that are unused to free as much memory as possible.
    for (auto i = 0U; i <= system::topology::max_node_id(); ++i)
    {
        auto &numa_blocks = this->_numa_allocation_blocks[i];
        numa_blocks.erase(
            std::remove_if(numa_blocks.begin(), numa_blocks.end(), [](const auto &block) { return block.is_free(); }),
            numa_blocks.end());
    }

    // If all memory was released, acquire new.
    this->initialize_empty();
}

void Allocator::initialize_empty()
{
    // For performance reasons: Each list must contain at least
    // one block. This way, we do not have to check every time.
    for (auto i = 0U; i <= system::topology::max_node_id(); ++i)
    {
        auto &blocks = this->_numa_allocation_blocks[i];
        if (blocks.empty())
        {
            const auto next_id = this->_next_allocation_id[i].value().fetch_add(1U, std::memory_order_relaxed);
            blocks.emplace_back(AllocationBlock{next_id, std::uint8_t(i), 4096U * 4096U});
        }
    }
}

bool Allocator::is_free() const noexcept
{
    for (auto i = 0U; i <= system::topology::max_node_id(); ++i)
    {
        const auto &numa_blocks = this->_numa_allocation_blocks[i];
        const auto iterator = std::find_if(numa_blocks.cbegin(), numa_blocks.cend(), [](const auto &allocation_block) {
            return allocation_block.is_free() == false;
        });

        if (iterator != numa_blocks.cend())
        {
            return false;
        }
    }

    return true;
}

void Allocator::release_allocated_memory() noexcept
{
    for (auto i = 0U; i <= system::topology::max_node_id(); ++i)
    {
        this->_numa_allocation_blocks[i].clear();
        this->_next_allocation_id[i].value().store(0U);
    }
}