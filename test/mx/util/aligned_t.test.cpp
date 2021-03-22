#include <gtest/gtest.h>
#include <mx/util/aligned_t.h>

TEST(MxTasking, aligned_t)
{
    EXPECT_EQ(sizeof(mx::util::aligned_t<std::uint8_t>), 64U);
    EXPECT_EQ(sizeof(mx::util::aligned_t<std::uint64_t>), 64U);

    auto aligned_value = mx::util::aligned_t<std::uint64_t>{42U};
    EXPECT_EQ(aligned_value.value(), 42U);

    aligned_value = 1337U;
    EXPECT_EQ(aligned_value.value(), 1337U);
}