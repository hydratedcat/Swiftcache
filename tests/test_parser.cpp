#include <gtest/gtest.h>

#include "../src/parser/command.h"

// =============================================================================
// SET
// =============================================================================

TEST(ParserTest, ParseSetValid) {
  auto cmd = CommandParser::parse("SET mykey myvalue");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "mykey");
  EXPECT_EQ(cmd.args[1], "myvalue");
}

TEST(ParserTest, ParseSetMissingValue) {
  auto cmd = CommandParser::parse("SET mykey");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_FALSE(cmd.valid);
  EXPECT_FALSE(cmd.error_msg.empty());
}

TEST(ParserTest, ParseSetMissingKeyAndValue) {
  auto cmd = CommandParser::parse("SET");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_FALSE(cmd.valid);
}

// =============================================================================
// GET
// =============================================================================

TEST(ParserTest, ParseGetValid) {
  auto cmd = CommandParser::parse("GET somekey");
  EXPECT_EQ(cmd.type, CommandType::GET);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "somekey");
}

TEST(ParserTest, ParseGetMissingKey) {
  auto cmd = CommandParser::parse("GET");
  EXPECT_EQ(cmd.type, CommandType::GET);
  EXPECT_FALSE(cmd.valid);
}

// =============================================================================
// DEL
// =============================================================================

TEST(ParserTest, ParseDelValid) {
  auto cmd = CommandParser::parse("DEL targetkey");
  EXPECT_EQ(cmd.type, CommandType::DEL);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "targetkey");
}

TEST(ParserTest, ParseDelMissingKey) {
  auto cmd = CommandParser::parse("DEL");
  EXPECT_EQ(cmd.type, CommandType::DEL);
  EXPECT_FALSE(cmd.valid);
}

// =============================================================================
// TTL
// =============================================================================

TEST(ParserTest, ParseTTLValid) {
  auto cmd = CommandParser::parse("TTL mykey 60");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "mykey");
  EXPECT_EQ(cmd.args[1], "60");
}

TEST(ParserTest, ParseTTLNonIntegerSeconds) {
  auto cmd = CommandParser::parse("TTL mykey notanumber");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_FALSE(cmd.valid);
}

TEST(ParserTest, ParseTTLNegativeSeconds) {
  auto cmd = CommandParser::parse("TTL mykey -5");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_FALSE(cmd.valid);
}

TEST(ParserTest, ParseTTLMissingSeconds) {
  auto cmd = CommandParser::parse("TTL mykey");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_FALSE(cmd.valid);
}

// =============================================================================
// EXISTS
// =============================================================================

TEST(ParserTest, ParseExistsValid) {
  auto cmd = CommandParser::parse("EXISTS somekey");
  EXPECT_EQ(cmd.type, CommandType::EXISTS);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "somekey");
}

// =============================================================================
// PING
// =============================================================================

TEST(ParserTest, ParsePing) {
  auto cmd = CommandParser::parse("PING");
  EXPECT_EQ(cmd.type, CommandType::PING);
  EXPECT_TRUE(cmd.valid);
}

// PING with trailing garbage is still valid (server ignores extra tokens)
TEST(ParserTest, ParsePingWithExtraTokens) {
  auto cmd = CommandParser::parse("PING extra ignored");
  EXPECT_EQ(cmd.type, CommandType::PING);
  EXPECT_TRUE(cmd.valid);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(ParserTest, ParseEmptyInput) {
  auto cmd = CommandParser::parse("");
  EXPECT_EQ(cmd.type, CommandType::UNKNOWN);
  EXPECT_FALSE(cmd.valid);
}

TEST(ParserTest, ParseWhitespaceOnly) {
  auto cmd = CommandParser::parse("   ");
  EXPECT_EQ(cmd.type, CommandType::UNKNOWN);
  EXPECT_FALSE(cmd.valid);
}

TEST(ParserTest, ParseUnknownCommand) {
  auto cmd = CommandParser::parse("FLUSHALL");
  EXPECT_EQ(cmd.type, CommandType::UNKNOWN);
  EXPECT_FALSE(cmd.valid);
  EXPECT_FALSE(cmd.error_msg.empty());
}

TEST(ParserTest, CaseInsensitiveCommand) {
  auto cmd = CommandParser::parse("set foo bar");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_TRUE(cmd.valid);
}

TEST(ParserTest, MixedCaseCommand) {
  auto cmd = CommandParser::parse("pInG");
  EXPECT_EQ(cmd.type, CommandType::PING);
  EXPECT_TRUE(cmd.valid);
}
