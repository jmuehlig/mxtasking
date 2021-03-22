#include <gtest/gtest.h>
#include <mx/util/core_set.h>

TEST(MxTasking, core_set)
{
    auto core_set = mx::util::core_set{};
    EXPECT_FALSE(static_cast<bool>(core_set));
    EXPECT_EQ(core_set.size(), 0U);

    core_set.emplace_back(0U);
    EXPECT_TRUE(static_cast<bool>(core_set));
    EXPECT_EQ(core_set.size(), 1U);
    EXPECT_EQ(core_set[0], 0U);
    EXPECT_EQ(core_set.max_core_id(), 0U);

    core_set.emplace_back(2U);
    EXPECT_TRUE(static_cast<bool>(core_set));
    EXPECT_EQ(core_set.size(), 2U);
    EXPECT_EQ(core_set[0], 0U);
    EXPECT_EQ(core_set[1], 2U);
    EXPECT_EQ(core_set.max_core_id(), 2U);

    core_set.emplace_back(1U);
    EXPECT_TRUE(static_cast<bool>(core_set));
    EXPECT_EQ(core_set.size(), 3U);
    EXPECT_EQ(core_set[0], 0U);
    EXPECT_EQ(core_set[1], 2U);
    EXPECT_EQ(core_set[2], 1U);
    EXPECT_EQ(core_set.max_core_id(), 2U);
}