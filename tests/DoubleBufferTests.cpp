#include <gtest/gtest.h>
#include <DoubleBuffer.hpp>

TEST(BasicTests, InitialValue) {
    yy::DoubleBuffer<int> buffer(42);
    EXPECT_EQ(buffer.read(), 42);
}

TEST(BasicTests, WriteReadConsistency) {
    yy::DoubleBuffer<std::string> buffer("init");
    EXPECT_EQ(buffer.read(), "init");
    buffer.write("updated");
    EXPECT_EQ(buffer.read(), "updated");
}