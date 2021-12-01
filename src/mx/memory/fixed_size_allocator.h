#pragma once

#include "alignment_helper.h"
#include "config.h"
#include "global_heap.h"
#include "task_allocator_interface.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mx/synchronization/spinlock.h>
#include <mx/system/cache.h>
#include <mx/system/topology.h>
#include <mx/tasking/config.h>
#include <mx/util/core_set.h>
#include <unordered_map>
#include <vector>

namespace mx::memory::fixed {
/**
 * Represents a free memory object.
 */
class FreeHeader
{
public:
    constexpr FreeHeader() noexcept = default;
    ~FreeHeader() noexcept = default;

    [[nodiscard]] FreeHeader *next() const noexcept { return _next; }
    void next(FreeHeader *next) noexcept { _next = next; }

    void numa_node_id(const std::uint8_t numa_node_id) noexcept { _numa_node_id = numa_node_id; }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return _numa_node_id; }

private:
    FreeHeader *_next = nullptr;
    std::uint8_t _numa_node_id = 0U;
};

/**
 * The Chunk holds a fixed size of memory.
 */
class Chunk
{
public:
    Chunk() noexcept = default;
    explicit Chunk(void *memory) noexcept : _memory(memory) {}
    ~Chunk() noexcept = default;

    static constexpr auto size() { return 4096 * 4096; /* 16mb */ }

    explicit operator void *() const noexcept { return _memory; }
    explicit operator std::uintptr_t() const noexcept { return reinterpret_cast<std::uintptr_t>(_memory); }
    explicit operator bool() const noexcept { return _memory != nullptr; }

private:
    void *_memory{nullptr};
};

/**
 * The ProcessorHeap holds memory for a single socket.
 * All cores sitting on this socket can allocate memory.
 * Internal, the ProcessorHeap bufferes allocated memory
 * to minimize access to the global heap.
 */
class alignas(64) ProcessorHeap
{
public:
    ProcessorHeap() noexcept = default;

    explicit ProcessorHeap(const std::uint8_t numa_node_id) noexcept : _numa_node_id(numa_node_id)
    {
        _allocated_chunks.reserve(1024);
        fill_buffer<true>();
    }

    ~ProcessorHeap() noexcept
    {
        for (const auto allocated_chunk : _allocated_chunks)
        {
            GlobalHeap::free(static_cast<void *>(allocated_chunk), Chunk::size());
        }

        for (const auto free_chunk : _free_chunk_buffer)
        {
            if (static_cast<bool>(free_chunk))
            {
                GlobalHeap::free(static_cast<void *>(free_chunk), Chunk::size());
            }
        }
    }

    ProcessorHeap &operator=(ProcessorHeap &&other) noexcept
    {
        _numa_node_id = std::exchange(other._numa_node_id, std::numeric_limits<std::uint8_t>::max());
        _free_chunk_buffer = other._free_chunk_buffer;
        other._free_chunk_buffer.fill(Chunk{});
        _next_free_chunk.store(other._next_free_chunk.load());
        _fill_buffer_flag.store(other._fill_buffer_flag.load());
        _allocated_chunks = std::move(other._allocated_chunks);
        return *this;
    }

    /**
     * @return ID of the NUMA node the memory is allocated on.
     */
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return _numa_node_id; }

    /**
     * Allocates a chunk of memory from the internal buffer.
     * In case the buffer is empty, new Chunks from the GlobalHeap
     * will be allocated.
     *
     * @return A chunk of allocated memory.
     */
    Chunk allocate() noexcept
    {
        const auto next_free_chunk = _next_free_chunk.fetch_add(1, std::memory_order_relaxed);
        if (next_free_chunk < _free_chunk_buffer.size())
        {
            return _free_chunk_buffer[next_free_chunk];
        }

        auto expect = false;
        const auto can_fill = _fill_buffer_flag.compare_exchange_strong(expect, true);
        if (can_fill)
        {
            fill_buffer<false>();
            _fill_buffer_flag = false;
        }
        else
        {
            while (_fill_buffer_flag)
            {
                system::builtin::pause();
            }
        }

        return allocate();
    }

private:
    // Size of the internal chunk buffer.
    inline static constexpr auto CHUNKS = 128U;

    // ID of the NUMA node of this ProcessorHeap.
    std::uint8_t _numa_node_id{std::numeric_limits<std::uint8_t>::max()};

    // Buffer for free chunks.
    std::array<Chunk, CHUNKS> _free_chunk_buffer;

    // Pointer to the next free chunk in the buffer.
    alignas(64) std::atomic_uint8_t _next_free_chunk{0U};

    // Flag, used for allocation from the global Heap for mutual exclusion.
    std::atomic_bool _fill_buffer_flag{false};

    // List of all allocated chunks, they will be freed later.
    std::vector<Chunk> _allocated_chunks;

    /**
     * Allocates a very big chunk from the GlobalHeap and
     * splits it into smaller chunks to store them in the
     * internal buffer.
     */
    template <bool IS_FIRST = false> void fill_buffer() noexcept
    {
        if constexpr (IS_FIRST == false)
        {
            for (const auto &chunk : _free_chunk_buffer)
            {
                _allocated_chunks.push_back(chunk);
            }
        }

        auto *heap_memory = GlobalHeap::allocate(_numa_node_id, Chunk::size() * _free_chunk_buffer.size());
        auto heap_memory_address = reinterpret_cast<std::uintptr_t>(heap_memory);
        for (auto i = 0U; i < _free_chunk_buffer.size(); ++i)
        {
            _free_chunk_buffer[i] = Chunk(reinterpret_cast<void *>(heap_memory_address + (i * Chunk::size())));
        }

        _next_free_chunk.store(0U);
    }
};

/**
 * The CoreHeap represents the allocator on a single core.
 * By this, allocations are latch-free.
 */
template <std::size_t S> class alignas(64) CoreHeap
{
public:
    explicit CoreHeap(ProcessorHeap *processor_heap) noexcept : _processor_heap(processor_heap) { fill_buffer(); }

    CoreHeap() noexcept = default;

    ~CoreHeap() noexcept = default;

    /**
     * Allocates new memory from the CoreHeap.
     * When the internal buffer is empty, the CoreHeap
     * will allocate new chunks from the ProcessorHeap.
     *
     * @return Pointer to the new allocated memory.
     */
    [[nodiscard]] void *allocate() noexcept
    {
        if (empty())
        {
            fill_buffer();
        }

        auto *free_element = std::exchange(_first, _first->next());
        return static_cast<void *>(free_element);
    }

    /**
     * Frees a memory object. The new available memory location
     * will be placed in front of the "available"-list. By this,
     * the next allocation will use the just freed object, which
     * may be still in the CPU cache.
     *
     * @param pointer Pointer to the memory object to be freed.
     */
    void free(void *pointer) noexcept
    {
        auto *free_object = static_cast<FreeHeader *>(pointer);
        free_object->next(_first);
        _first = free_object;
    }

    /**
     * Fills the buffer by asking the ProcessorHeap for more memory.
     * This is latch-free since just a single core calls this method.
     */
    void fill_buffer()
    {
        auto chunk = _processor_heap->allocate();
        const auto chunk_address = static_cast<std::uintptr_t>(chunk);

        constexpr auto object_size = S;
        constexpr auto count_objects = std::uint64_t{Chunk::size() / object_size};

        auto *first_free = reinterpret_cast<FreeHeader *>(chunk_address);
        auto *last_free = reinterpret_cast<FreeHeader *>(chunk_address + ((count_objects - 1) * object_size));

        auto *current_free = first_free;
        for (auto i = 0U; i < count_objects - 1U; ++i)
        {
            auto *next = reinterpret_cast<FreeHeader *>(chunk_address + ((i + 1U) * object_size));
            current_free->next(next);
            current_free = next;
        }

        last_free->next(nullptr);
        _first = first_free;
    }

private:
    // Processor heap to allocate new chunks.
    ProcessorHeap *_processor_heap{nullptr};

    // First element of the list of free memory objects.
    FreeHeader *_first{nullptr};

    /**
     * @return True, when the buffer is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _first == nullptr; }
};

/**
 * The Allocator is the interface to the internal CoreHeaps.
 */
template <std::size_t S> class Allocator final : public TaskAllocatorInterface
{
public:
    explicit Allocator(const util::core_set &core_set)
    {
        for (auto node_id = std::uint8_t(0U); node_id < config::max_numa_nodes(); ++node_id)
        {
            if (core_set.has_core_of_numa_node(node_id))
            {
                _processor_heaps[node_id] = ProcessorHeap{node_id};
            }
        }

        for (const auto core_id : core_set)
        {
            const auto node_id = system::topology::node_id(core_id);
            _core_heaps[core_id] = CoreHeap<S>{&_processor_heaps[node_id]};
        }
    }

    ~Allocator() override = default;

    /**
     * Allocates memory from the given CoreHeap.
     *
     * @param core_id ID of the core.
     * @return Allocated memory object.
     */
    [[nodiscard]] void *allocate(const std::uint16_t core_id) override { return _core_heaps[core_id].allocate(); }

    /**
     * Frees memory.
     *
     * @param core_id ID of the core to place the free object in.
     * @param address Pointer to the memory object.
     */
    void free(const std::uint16_t core_id, void *address) noexcept override { _core_heaps[core_id].free(address); }

private:
    // Heap for every processor socket/NUMA region.
    std::array<ProcessorHeap, config::max_numa_nodes()> _processor_heaps;

    // Map from core_id to core-local allocator.
    std::array<CoreHeap<S>, tasking::config::max_cores()> _core_heaps;
};
} // namespace mx::memory::fixed