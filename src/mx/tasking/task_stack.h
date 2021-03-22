#pragma once

#include "config.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <mx/system/environment.h>
#ifdef USE_SSE2
#include <emmintrin.h>
#endif

namespace mx::tasking {
/**
 * Stack to save/restore tasks before/after optimistic synchronization.
 * In case of failed read, the task will be restored to re-run.
 */
class TaskInterface;
class TaskStack
{
public:
    constexpr TaskStack() : _data({}) { _data.fill(std::byte{'\0'}); }
    ~TaskStack() = default;

    /**
     * Saves the full task on the stack.
     * @param task Task to save.
     */
    void save(const TaskInterface *task) noexcept
    {
        if constexpr (system::Environment::is_sse2() && (config::task_size() == 64U || config::task_size() == 128U))
        {
            TaskStack::memcpy_simd<config::task_size()>(_data.data(), static_cast<const void *>(task));
        }
        else if constexpr (config::task_size() == 64U || config::task_size() == 128U)
        {
            TaskStack::memcpy_tiny<config::task_size()>(_data.data(), static_cast<const void *>(task));
        }
        else
        {
            std::memcpy(_data.data(), static_cast<const void *>(task), config::task_size());
        }
    }

    /**
     * Restores the full task from the stack.
     * @param task Task to restore.
     */
    void restore(TaskInterface *task) const noexcept
    {
        if constexpr (system::Environment::is_sse2() && (config::task_size() == 64U || config::task_size() == 128U))
        {
            TaskStack::memcpy_simd<config::task_size()>(static_cast<void *>(task), _data.data());
        }
        else if constexpr (config::task_size() == 64U || config::task_size() == 128U)
        {
            TaskStack::memcpy_tiny<config::task_size()>(static_cast<void *>(task), _data.data());
        }
        else
        {
            std::memcpy(static_cast<void *>(task), _data.data(), config::task_size());
        }
    }

    /**
     * Saves some data on the stack.
     *
     * @param index Index where to store.
     * @param data Data to store.
     */
    template <typename T> void store(const std::uint16_t index, const T &data)
    {
        *reinterpret_cast<T *>(&_data[index]) = data;
    }

    /**
     * Restores some data from the stack.
     *
     * @param index Index where the data is stored.
     * @return The restored data.
     */
    template <typename T> const T *read(const std::uint16_t index) const
    {
        return reinterpret_cast<const T *>(&_data[index]);
    }

private:
    // Data to store tasks or single data on the stack.
    std::array<std::byte, config::task_size()> _data;

    template <std::size_t S>
    static inline void memcpy_simd([[maybe_unused]] void *destination, [[maybe_unused]] const void *src)
    {
#ifdef USE_SSE2
        if constexpr (S == 64U)
        {
            __m128i m0 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 0U);
            __m128i m1 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 1U);
            __m128i m2 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 2U);
            __m128i m3 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 3U);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 0U, m0);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 1U, m1);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 2U, m2);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 3U, m3);
        }
        else if constexpr (S == 128U)
        {
            __m128i m0 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 0U);
            __m128i m1 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 1U);
            __m128i m2 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 2U);
            __m128i m3 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 3U);
            __m128i m4 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 4U);
            __m128i m5 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 5U);
            __m128i m6 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 6U);
            __m128i m7 = _mm_loadu_si128(static_cast<const __m128i *>(src) + 7U);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 0U, m0);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 1U, m1);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 2U, m2);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 3U, m3);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 4U, m4);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 5U, m5);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 6U, m6);
            _mm_storeu_si128(static_cast<__m128i *>(destination) + 7U, m7);
        }
#endif
    }

    template <std::size_t S>
    static inline void memcpy_tiny([[maybe_unused]] void *destination, [[maybe_unused]] const void *src)
    {
        if constexpr (S == 64U)
        {
            static_cast<std::int64_t *>(destination)[0U] = static_cast<const std::int64_t *>(src)[0U];
            static_cast<std::int64_t *>(destination)[1U] = static_cast<const std::int64_t *>(src)[1U];
            static_cast<std::int64_t *>(destination)[2U] = static_cast<const std::int64_t *>(src)[2U];
            static_cast<std::int64_t *>(destination)[3U] = static_cast<const std::int64_t *>(src)[3U];
            static_cast<std::int64_t *>(destination)[4U] = static_cast<const std::int64_t *>(src)[4U];
            static_cast<std::int64_t *>(destination)[5U] = static_cast<const std::int64_t *>(src)[5U];
            static_cast<std::int64_t *>(destination)[6U] = static_cast<const std::int64_t *>(src)[6U];
            static_cast<std::int64_t *>(destination)[7U] = static_cast<const std::int64_t *>(src)[7U];
        }
        else if constexpr (S == 128U)
        {
            static_cast<std::int64_t *>(destination)[0U] = static_cast<const std::int64_t *>(src)[0U];
            static_cast<std::int64_t *>(destination)[1U] = static_cast<const std::int64_t *>(src)[1U];
            static_cast<std::int64_t *>(destination)[2U] = static_cast<const std::int64_t *>(src)[2U];
            static_cast<std::int64_t *>(destination)[3U] = static_cast<const std::int64_t *>(src)[3U];
            static_cast<std::int64_t *>(destination)[4U] = static_cast<const std::int64_t *>(src)[4U];
            static_cast<std::int64_t *>(destination)[5U] = static_cast<const std::int64_t *>(src)[5U];
            static_cast<std::int64_t *>(destination)[6U] = static_cast<const std::int64_t *>(src)[6U];
            static_cast<std::int64_t *>(destination)[7U] = static_cast<const std::int64_t *>(src)[7U];
            static_cast<std::int64_t *>(destination)[8U] = static_cast<const std::int64_t *>(src)[8U];
            static_cast<std::int64_t *>(destination)[9U] = static_cast<const std::int64_t *>(src)[9U];
            static_cast<std::int64_t *>(destination)[10U] = static_cast<const std::int64_t *>(src)[10U];
            static_cast<std::int64_t *>(destination)[11U] = static_cast<const std::int64_t *>(src)[11U];
            static_cast<std::int64_t *>(destination)[12U] = static_cast<const std::int64_t *>(src)[12U];
            static_cast<std::int64_t *>(destination)[13U] = static_cast<const std::int64_t *>(src)[13U];
            static_cast<std::int64_t *>(destination)[14U] = static_cast<const std::int64_t *>(src)[14U];
            static_cast<std::int64_t *>(destination)[15U] = static_cast<const std::int64_t *>(src)[15U];
        }
    }
};
} // namespace mx::tasking