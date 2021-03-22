#include <gtest/gtest.h>
#include <mx/memory/dynamic_size_allocator.h>

TEST(MxTasking, DynamicSizeAllocator)
{

    auto allocator = mx::memory::dynamic::Allocator{};

    // Allocation success
    auto *m1 = allocator.allocate(0U, 64U, sizeof(std::uint32_t));
    EXPECT_NE(m1, nullptr);

    // Alignment
    EXPECT_TRUE((std::uintptr_t(m1) & 0x3F) == 0U);

    // is free
    EXPECT_FALSE(allocator.is_free());
    allocator.free(m1);
    EXPECT_TRUE(allocator.is_free());

    // Different allocations, different blocks
    EXPECT_NE(allocator.allocate(0U, 64U, sizeof(std::uint32_t)), allocator.allocate(0U, 64U, sizeof(std::uint32_t)));
}