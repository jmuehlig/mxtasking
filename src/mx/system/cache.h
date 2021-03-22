#pragma once

#include <cstdint>

namespace mx::system {
/**
 * Encapsulates cache operations like prefetching.
 *
 * Further documentation on Intel: https://www.felixcloutier.com/x86/prefetchh
 */
class cache
{
public:
    enum level : std::uint8_t
    {
        L1 = 1U,
        L2 = 2U,
        LLC = 3U
    };
    enum access : std::uint8_t
    {
        read = 0U,
        write = 1U
    };

    /**
     * Prefetches a single cache line into a given prefetch level.
     *
     * @tparam L Wanted cache level.
     * @tparam A Access to the cache line whether read or write.
     * @param address Address of the memory which should be prefetched.
     */
    template <level L, access A = access::read> static void prefetch(void *address) noexcept
    {
#ifdef __x86_64
        if constexpr (A == access::write)
        {
            asm volatile("PREFETCHW (%0)\n" ::"r"(address));
        }
        else if constexpr (L == level::L1)
        {
            asm volatile("PREFETCHT1 (%0)\n" ::"r"(address));
        }
        else if constexpr (L == level::L2)
        {
            asm volatile("PREFETCHT2 (%0)\n" ::"r"(address));
        }
        else
        {
            asm volatile("PREFETCHNTA (%0)\n" ::"r"(address));
        }
#elif defined(__aarch64__)
        if constexpr (L == L1)
        {
            if constexpr (A == access::read)
            {
                asm volatile("prfm pldl1keep, %a0\n" : : "p"(address));
            }
            else
            {
                asm volatile("prfm pstl1keep, %a0\n" : : "p"(address));
            }
        }
        else if constexpr (L == L2)
        {
            if constexpr (A == access::read)
            {
                asm volatile("prfm pldl2keep, %a0\n" : : "p"(address));
            }
            else
            {
                asm volatile("prfm pstl2keep, %a0\n" : : "p"(address));
            }
        }
        else
        {
            if constexpr (A == access::read)
            {
                asm volatile("prfm pldl3keep, %a0\n" : : "p"(address));
            }
            else
            {
                asm volatile("prfm pstl3keep, %a0\n" : : "p"(address));
            }
        }
#endif
    }

    /**
     * Prefetches a range of cache lines into the given cache level.
     *
     * @tparam L Wanted cache level.
     * @tparam A Access to the cache line whether read or write.
     * @param address Address of the memory which should be prefetched.
     * @param size Size of the accessed memory.
     */
    template <level L, access A = access::read>
    static void prefetch_range(void *address, const std::uint32_t size) noexcept
    {
        auto addr = std::uintptr_t(address);
        const auto end = addr + size;

        if ((size & 1023U) == 0U)
        {
            for (; addr < end; addr += 1024U)
            {
                prefetch_range<L, 1024U, A>(reinterpret_cast<void *>(addr));
            }
        }
        else if ((size & 511U) == 0U)
        {
            for (; addr < end; addr += 512U)
            {
                prefetch_range<L, 512U, A>(reinterpret_cast<void *>(addr));
            }
        }
        else if ((size & 255U) == 0U)
        {
            for (; addr < end; addr += 256U)
            {
                prefetch_range<L, 256U, A>(reinterpret_cast<void *>(addr));
            }
        }
        else if ((size & 127U) == 0U)
        {
            for (; addr < end; addr += 128U)
            {
                prefetch_range<L, 128U, A>(reinterpret_cast<void *>(addr));
            }
        }
        else
        {
            for (; addr < end; addr += 64U)
            {
                prefetch<L, A>(reinterpret_cast<void *>(addr));
            }
        }
    }

    /**
     * Prefetches a range of cache lines into the given cache level.
     *
     * @tparam L Wanted cache level.
     * @tparam S Size of the accessed memory.
     * @tparam A Access to the cache line whether read or write.
     * @param address Address of the accessed memory.
     */
    template <level L, std::uint32_t S, access A = access::read> static void prefetch_range(void *address) noexcept
    {
        static_assert(S && (!(S & (S - 1))) && "Must be power of two.");
        const auto addr = std::uintptr_t(address);
        if constexpr (S <= 64U)
        {
            prefetch<L, A>(address);
        }
        else if constexpr (S == 128U)
        {
            prefetch<L, A>(address);
            prefetch<L, A>(reinterpret_cast<void *>(addr + 64U));
        }
        else if constexpr (S == 192U)
        {
            prefetch_range<L, 128U, A>(address);
            prefetch<L, A>(reinterpret_cast<void *>(addr + 128U));
        }
        else if constexpr (S == 256U)
        {
            prefetch_range<L, 128U, A>(address);
            prefetch_range<L, 128U, A>(reinterpret_cast<void *>(addr + 128U));
        }
        else if constexpr (S == 512U)
        {
            prefetch_range<L, 256U, A>(address);
            prefetch_range<L, 256U, A>(reinterpret_cast<void *>(addr + 256U));
        }
        else if constexpr (S == 1024U)
        {
            prefetch_range<L, 512U, A>(address);
            prefetch_range<L, 512U, A>(reinterpret_cast<void *>(addr + 512U));
        }
        else
        {
            prefetch_range<L, A>(address, S);
        }
    }
};
} // namespace mx::system