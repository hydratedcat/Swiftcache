#include <gtest/gtest.h>
#include "../src/parser/command.h"

TEST(ParserTest, Placeholder) {
    auto cmd = CommandParser::parse("UNKNOWN");
    EXPECT_EQ(cmd.type, CommandType::UNKNOWN);
}
