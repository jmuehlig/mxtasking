#include <gtest/gtest.h>
#include <mx/util/vector.h>

TEST(MxTasking, vector)
{
    auto items = mx::util::vector<std::uint64_t, std::uint32_t>{};
    EXPECT_TRUE(items.empty());
    EXPECT_EQ(items.size(), 0U);

    items.emplace_back(42U);
    EXPECT_FALSE(items.empty());
    EXPECT_EQ(items.size(), 1U);
    EXPECT_EQ(items[0], 42U);

    items.clear();
    EXPECT_TRUE(items.empty());

    for (auto i = 0U; i < 1024U; ++i)
    {
        items.emplace_back(i + 1U);
    }
    EXPECT_EQ(items.size(), 1024U);
    EXPECT_EQ(items[0], 1U);
    EXPECT_EQ(items[1023], 1024U);

    EXPECT_EQ(sizeof(decltype(items)), 24U);

    for (const auto &i : items)
    {
        EXPECT_TRUE(i > 0U);
    }

    items[0] = 1337U;
    EXPECT_EQ(items[0], 1337U);
}