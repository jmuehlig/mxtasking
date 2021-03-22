#include <gtest/gtest.h>
#include <mx/memory/tagged_ptr.h>

TEST(MxTasking, tagged_ptr)
{
    auto p = std::uint32_t{42U};
    auto i = std::uint16_t{1337U};
    auto ptr = mx::memory::tagged_ptr<std::uint32_t, std::uint16_t>{};

    EXPECT_EQ(ptr.get(), nullptr);

    ptr.reset(&p);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr.get(), &p);
    EXPECT_EQ(*ptr.get(), p);

    ptr.reset(i);
    EXPECT_EQ(ptr.info(), i);

    auto ptr2 = mx::memory::tagged_ptr<std::uint32_t, std::uint16_t>{&p};
    EXPECT_EQ(ptr, ptr2);
    ptr2.reset(i);
    EXPECT_EQ(ptr, ptr2);

    ptr.reset();
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr, nullptr);
    EXPECT_NE(ptr, ptr2);
}