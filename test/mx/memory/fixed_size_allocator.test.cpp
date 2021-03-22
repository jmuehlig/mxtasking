#include <gtest/gtest.h>
#include <mx/memory/fixed_size_allocator.h>

TEST(MxTasking, FixedSizeAllocator)
{
    // Single core
    {
        auto core_set = mx::util::core_set{};
        core_set.emplace_back(0U);

        auto allocator = mx::memory::fixed::Allocator<64U>{core_set};

        // Allocation success
        auto *m1 = allocator.allocate(0U);
        EXPECT_NE(m1, nullptr);

        // Alignment
        EXPECT_TRUE((std::uintptr_t(m1) & 0x3F) == 0U);

        // Allocate same block after free
        allocator.free(0U, m1);
        EXPECT_EQ(allocator.allocate(0U), m1);

        // Different allocations, different blocks
        EXPECT_NE(allocator.allocate(0U), allocator.allocate(0U));
    }

    // Different cores
    {
        auto core_set = mx::util::core_set{};
        core_set.emplace_back(0U);
        core_set.emplace_back(1U);

        auto allocator = mx::memory::fixed::Allocator<64U>{core_set};

        // Allocation success
        auto *m1 = allocator.allocate(1U);
        EXPECT_NE(m1, nullptr);

        // Alignment
        EXPECT_TRUE((std::uintptr_t(m1) & 0x3F) == 0);

        // Allocate same block after free
        allocator.free(1U, m1);
        EXPECT_EQ(allocator.allocate(1U), m1);

        // Different blocks of different cores
        EXPECT_NE(allocator.allocate(0U), allocator.allocate(1U));

        // Move block to different core
        auto *m2 = allocator.allocate(0U);
        allocator.free(1U, m2);
        EXPECT_EQ(allocator.allocate(1U), m2);
    }
}