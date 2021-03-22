#include <gtest/gtest.h>
#include <mx/memory/alignment_helper.h>

TEST(MxTasking, alignment_helper)
{
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(4U, 64U), 64U);
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(64U, 64U), 64U);
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(65U, 64U), 128U);
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(128U, 64U), 128U);
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(129U, 64U), 192U);
    EXPECT_EQ(mx::memory::alignment_helper::next_multiple(180U, 64U), 192U);

    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(4U));
    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(8U));
    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(16U));
    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(32U));
    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(64U));
    EXPECT_TRUE(mx::memory::alignment_helper::is_power_of_two(128U));
    EXPECT_FALSE(mx::memory::alignment_helper::is_power_of_two(3U));
    EXPECT_FALSE(mx::memory::alignment_helper::is_power_of_two(6U));
    EXPECT_FALSE(mx::memory::alignment_helper::is_power_of_two(15U));
    EXPECT_FALSE(mx::memory::alignment_helper::is_power_of_two(17U));
    EXPECT_FALSE(mx::memory::alignment_helper::is_power_of_two(100U));

    EXPECT_EQ(mx::memory::alignment_helper::next_power_of_two(3U), 4U);
    EXPECT_EQ(mx::memory::alignment_helper::next_power_of_two(17U), 32U);
    EXPECT_EQ(mx::memory::alignment_helper::next_power_of_two(64U), 64U);
    EXPECT_EQ(mx::memory::alignment_helper::next_power_of_two(132U), 256U);
    EXPECT_EQ(mx::memory::alignment_helper::next_power_of_two(255U), 256U);
}