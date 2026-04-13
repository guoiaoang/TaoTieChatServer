#include "gtest/gtest.h"

TEST(HelloWorldTest, BasicAssertions) {
	EXPECT_STREQ("Hello, World!", "Hello, World!");
	EXPECT_EQ(1 + 1, 2);
}
