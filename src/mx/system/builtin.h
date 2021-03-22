#pragma once
#include <cstdint>
#include <iostream>

namespace mx::system {
/**
 * Encapsulates compiler builtins.
 */
class builtin
{
public:
    /**
     * Generates a pause/yield cpu instruction, independently
     * of the hardware.
     */
    static void pause() noexcept
    {
#if defined(__x86_64__) || defined(__amd64__)
        __builtin_ia32_pause();
#elif defined(__arm__)
        asm("YIELD");
#endif
    }

    [[maybe_unused]] static bool expect_false(const bool expression) noexcept
    {
        return __builtin_expect(expression, false);
    }

    [[maybe_unused]] static bool expect_true(const bool expression) noexcept
    {
        return __builtin_expect(expression, true);
    }
};
} // namespace mx::system