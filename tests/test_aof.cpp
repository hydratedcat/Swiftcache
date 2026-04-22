#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../src/core/lru.h"
#include "../src/core/ttl.h"
#include "../src/parser/command.h"
#include "../src/persistence/aof.h"

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
const std::string TEST_AOF_PATH = "test_aof_temp.aof";
constexpr size_t TEST_CACHE_CAPACITY = 1000;
constexpr int NUM_CONCURRENT_WRITERS = 10;
constexpr int COMMANDS_PER_WRITER = 100;
} // namespace

// ---------------------------------------------------------------------------
// Helper: remove the test AOF file between tests
// ---------------------------------------------------------------------------
class AOFTest : public ::testing::Test {
protected:
  void SetUp() override { std::remove(TEST_AOF_PATH.c_str()); }
  void TearDown() override { std::remove(TEST_AOF_PATH.c_str()); }
};

// ---------------------------------------------------------------------------
// AOFWriter tests
// ---------------------------------------------------------------------------
TEST_F(AOFTest, LogCommandWritesToFile) {
  // Arrange
  {
    AOFWriter writer(TEST_AOF_PATH);
    // Act
    writer.log_command("SET name aman");
    writer.log_command("SET city delhi");
    writer.log_command("DEL city");
  } // writer closes file in destructor

  // Assert — read the file manually and verify contents
  std::ifstream file(TEST_AOF_PATH);
  ASSERT_TRUE(file.is_open());

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }

  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], "SET name aman");
  EXPECT_EQ(lines[1], "SET city delhi");
  EXPECT_EQ(lines[2], "DEL city");
}

TEST_F(AOFTest, AppendsAcrossMultipleWriterInstances) {
  // Arrange — first writer
  {
    AOFWriter writer(TEST_AOF_PATH);
    writer.log_command("SET key1 val1");
  }

  // Act — second writer appends to same file
  {
    AOFWriter writer(TEST_AOF_PATH);
    writer.log_command("SET key2 val2");
  }

  // Assert
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();
  ASSERT_EQ(commands.size(), 2u);
  EXPECT_EQ(commands[0], "SET key1 val1");
  EXPECT_EQ(commands[1], "SET key2 val2");
}

TEST_F(AOFTest, FsyncNowFlushesWithoutError) {
  AOFWriter writer(TEST_AOF_PATH);
  writer.log_command("SET test flush");
  // Should not throw or crash
  writer.fsync_now();
}

TEST_F(AOFTest, FilepathAccessor) {
  AOFWriter writer(TEST_AOF_PATH);
  EXPECT_EQ(writer.filepath(), TEST_AOF_PATH);
}

// ---------------------------------------------------------------------------
// AOFReader tests
// ---------------------------------------------------------------------------
TEST_F(AOFTest, ReadAllCommandsReturnsAllLines) {
  // Arrange — write a file manually
  {
    std::ofstream file(TEST_AOF_PATH);
    file << "SET name aman\n";
    file << "SET city delhi\n";
    file << "TTL name 60\n";
    file << "DEL city\n";
  }

  // Act
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();

  // Assert
  ASSERT_EQ(commands.size(), 4u);
  EXPECT_EQ(commands[0], "SET name aman");
  EXPECT_EQ(commands[1], "SET city delhi");
  EXPECT_EQ(commands[2], "TTL name 60");
  EXPECT_EQ(commands[3], "DEL city");
}

TEST_F(AOFTest, ReadAllCommandsSkipsEmptyLines) {
  // Arrange — file with blank lines
  {
    std::ofstream file(TEST_AOF_PATH);
    file << "SET a 1\n";
    file << "\n";
    file << "\n";
    file << "SET b 2\n";
    file << "\n";
  }

  // Act
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();

  // Assert — empty lines filtered out
  ASSERT_EQ(commands.size(), 2u);
  EXPECT_EQ(commands[0], "SET a 1");
  EXPECT_EQ(commands[1], "SET b 2");
}

TEST_F(AOFTest, ReadAllCommandsReturnsEmptyForMissingFile) {
  // Act — file does not exist
  AOFReader reader("nonexistent_file.aof");
  std::vector<std::string> commands = reader.read_all_commands();

  // Assert — graceful empty return, no crash
  EXPECT_TRUE(commands.empty());
}

TEST_F(AOFTest, ReadAllCommandsHandlesCRLF) {
  // Arrange — file with \r\n line endings
  {
    std::ofstream file(TEST_AOF_PATH, std::ios::binary);
    file << "SET key val\r\n";
    file << "DEL key\r\n";
  }

  // Act
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();

  // Assert — \r stripped
  ASSERT_EQ(commands.size(), 2u);
  EXPECT_EQ(commands[0], "SET key val");
  EXPECT_EQ(commands[1], "DEL key");
}

// ---------------------------------------------------------------------------
// Integration: AOF replay restores cache state
// ---------------------------------------------------------------------------
TEST_F(AOFTest, ReplayRestoresCacheState) {
  // Arrange — simulate a previous server session that logged commands
  {
    AOFWriter writer(TEST_AOF_PATH);
    writer.log_command("SET name aman");
    writer.log_command("SET city delhi");
    writer.log_command("SET lang cpp");
    writer.log_command("DEL city");
  }

  // Act — simulate startup: read log, parse, execute against fresh cache
  LRUCache cache(TEST_CACHE_CAPACITY);
  TTLManager ttl;
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();

  for (const auto &cmd_str : commands) {
    Command cmd = CommandParser::parse(cmd_str);
    ASSERT_TRUE(cmd.valid) << "Failed to parse: " << cmd_str;

    switch (cmd.type) {
    case CommandType::SET:
      cache.put(cmd.args[0], cmd.args[1]);
      break;
    case CommandType::DEL:
      ttl.remove(cmd.args[0]);
      (void)cache.del(cmd.args[0]);
      break;
    case CommandType::TTL: {
      int seconds = std::stoi(cmd.args[1]);
      ttl.set_expiry(cmd.args[0], seconds);
      break;
    }
    default:
      break;
    }
  }

  // Assert — cache contains replayed state
  EXPECT_EQ(cache.get("name"), "aman");
  EXPECT_EQ(cache.get("lang"), "cpp");
  EXPECT_EQ(cache.get("city"), std::nullopt); // was DEL'd
  EXPECT_EQ(cache.size(), 2u);
}

TEST_F(AOFTest, ReplayWithTTLRestoresExpiry) {
  // Arrange
  {
    AOFWriter writer(TEST_AOF_PATH);
    writer.log_command("SET temp tempval");
    writer.log_command("TTL temp 1");
  }

  // Act — replay and then wait for TTL to expire
  LRUCache cache(TEST_CACHE_CAPACITY);
  TTLManager ttl;
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();

  for (const auto &cmd_str : commands) {
    Command cmd = CommandParser::parse(cmd_str);
    ASSERT_TRUE(cmd.valid);
    if (cmd.type == CommandType::SET) {
      cache.put(cmd.args[0], cmd.args[1]);
    } else if (cmd.type == CommandType::TTL) {
      ttl.set_expiry(cmd.args[0], std::stoi(cmd.args[1]));
    }
  }

  // Key should exist immediately after replay
  EXPECT_FALSE(ttl.is_expired("temp"));
  EXPECT_EQ(cache.get("temp"), "tempval");

  // Wait for TTL to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // Key should now be expired
  EXPECT_TRUE(ttl.is_expired("temp"));
}

// ---------------------------------------------------------------------------
// Concurrency: multiple threads logging simultaneously
// ---------------------------------------------------------------------------
TEST_F(AOFTest, ConcurrentLogCommandIsThreadSafe) {
  // Arrange
  {
    AOFWriter writer(TEST_AOF_PATH);

    // Act — 10 threads each writing 100 commands
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_CONCURRENT_WRITERS; ++t) {
      threads.emplace_back([&writer, t]() {
        for (int i = 0; i < COMMANDS_PER_WRITER; ++i) {
          writer.log_command("SET key" + std::to_string(t * COMMANDS_PER_WRITER + i) +
                             " val" + std::to_string(i));
        }
      });
    }

    for (auto &th : threads) {
      th.join();
    }
  }

  // Assert — all commands are present (order may vary across threads)
  AOFReader reader(TEST_AOF_PATH);
  std::vector<std::string> commands = reader.read_all_commands();
  EXPECT_EQ(commands.size(),
            static_cast<size_t>(NUM_CONCURRENT_WRITERS * COMMANDS_PER_WRITER));

  // Every line should start with "SET "
  for (const auto &cmd : commands) {
    EXPECT_TRUE(cmd.substr(0, 4) == "SET ") << "Unexpected line: " << cmd;
  }
}

TEST_F(AOFTest, OverwriteKeyReplayGetsLatestValue) {
  // Arrange — same key SET multiple times
  {
    AOFWriter writer(TEST_AOF_PATH);
    writer.log_command("SET name first");
    writer.log_command("SET name second");
    writer.log_command("SET name third");
  }

  // Act — replay
  LRUCache cache(TEST_CACHE_CAPACITY);
  AOFReader reader(TEST_AOF_PATH);
  for (const auto &cmd_str : reader.read_all_commands()) {
    Command cmd = CommandParser::parse(cmd_str);
    if (cmd.valid && cmd.type == CommandType::SET) {
      cache.put(cmd.args[0], cmd.args[1]);
    }
  }

  // Assert — last write wins
  EXPECT_EQ(cache.get("name"), "third");
  EXPECT_EQ(cache.size(), 1u);
}
