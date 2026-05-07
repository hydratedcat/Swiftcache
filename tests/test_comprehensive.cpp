// =============================================================================
// Day 17 — Comprehensive Tests
//
// Tests covering edge cases, boundary conditions, integration scenarios,
// and concurrency stress tests across all modules.
// =============================================================================

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../src/core/lru.h"
#include "../src/core/store.h"
#include "../src/core/ttl.h"
#include "../src/parser/command.h"
#include "../src/persistence/aof.h"

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
constexpr size_t SMALL_CAPACITY = 5;
constexpr size_t MEDIUM_CAPACITY = 100;
constexpr size_t LARGE_CAPACITY = 100000;
constexpr int SHORT_TTL_SECONDS = 1;
constexpr int SHORT_SLEEP_MS = 1100;
const std::string COMP_AOF_PATH = "test_comprehensive.aof";
} // namespace

// ===========================================================================
// Store — Edge Cases
// ===========================================================================

TEST(CompStoreTest, EmptyKeyAndValue) {
  // Edge case: empty strings are valid keys/values in a KV store
  Store store;
  store.set("", "emptykey");
  EXPECT_EQ(store.get(""), "emptykey");

  store.set("emptyval", "");
  EXPECT_EQ(store.get("emptyval"), "");
}

TEST(CompStoreTest, LargeValue) {
  // Boundary: store a large string (1 MB)
  Store store;
  std::string big(1024 * 1024, 'x');
  store.set("bigkey", big);
  EXPECT_EQ(store.get("bigkey"), big);
}

TEST(CompStoreTest, SpecialCharactersInKey) {
  Store store;
  store.set("key with spaces", "val1");
  store.set("key\twith\ttabs", "val2");
  store.set("key:with:colons", "val3");
  store.set("emoji🔥key", "val4");

  EXPECT_EQ(store.get("key with spaces"), "val1");
  EXPECT_EQ(store.get("key\twith\ttabs"), "val2");
  EXPECT_EQ(store.get("key:with:colons"), "val3");
  EXPECT_EQ(store.get("emoji🔥key"), "val4");
}

TEST(CompStoreTest, ConcurrentReadWriteInterleaved) {
  // Interleave writes and reads from different threads
  Store store;
  const int NUM_KEYS = 1000;
  std::atomic<int> errors{0};

  // Pre-populate
  for (int i = 0; i < NUM_KEYS; ++i) {
    store.set(std::to_string(i), "init");
  }

  // Writers overwrite existing keys
  std::vector<std::thread> threads;
  for (int t = 0; t < 5; ++t) {
    threads.emplace_back([&store, t, &errors]() {
      for (int i = 0; i < NUM_KEYS; ++i) {
        store.set(std::to_string(i), "writer" + std::to_string(t));
      }
    });
  }
  // Readers concurrently read
  for (int t = 0; t < 5; ++t) {
    threads.emplace_back([&store, &errors]() {
      for (int i = 0; i < NUM_KEYS; ++i) {
        auto val = store.get(std::to_string(i));
        // Value should always be some valid string, never corrupted
        if (!val.has_value()) {
          ++errors; // Key was somehow deleted — shouldn't happen
        }
      }
    });
  }

  for (auto &th : threads) {
    th.join();
  }
  EXPECT_EQ(errors.load(), 0);
  EXPECT_EQ(store.size(), static_cast<size_t>(NUM_KEYS));
}

TEST(CompStoreTest, RapidSetDeleteCycle) {
  // Set and immediately delete the same key many times
  Store store;
  for (int i = 0; i < 10000; ++i) {
    store.set("cycle", std::to_string(i));
    EXPECT_TRUE(store.del("cycle"));
    EXPECT_FALSE(store.exists("cycle"));
  }
  EXPECT_EQ(store.size(), 0u);
}

// ===========================================================================
// LRU — Edge Cases
// ===========================================================================

TEST(CompLRUTest, EvictionChainFullCapacity) {
  // Fill cache, then insert N more items — all original items evicted
  LRUCache cache(SMALL_CAPACITY);
  for (int i = 0; i < 5; ++i) {
    cache.put("old" + std::to_string(i), "val");
  }
  for (int i = 0; i < 5; ++i) {
    cache.put("new" + std::to_string(i), "val");
  }
  // All old keys should be evicted
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(cache.get("old" + std::to_string(i)), std::nullopt);
  }
  // All new keys should be present
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(cache.get("new" + std::to_string(i)), "val");
  }
}

TEST(CompLRUTest, UpdateExistingKeyPreservesCapacity) {
  // Updating an existing key should NOT increase size or cause eviction
  LRUCache cache(2);
  cache.put("a", "1");
  cache.put("b", "2");
  cache.put("a", "updated"); // update, not insert
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get("a"), "updated");
  EXPECT_EQ(cache.get("b"), "2"); // b should NOT be evicted
}

TEST(CompLRUTest, UpdateMakesKeyMostRecent) {
  // put() on existing key should make it MRU — evict the other
  LRUCache cache(2);
  cache.put("a", "1");
  cache.put("b", "2");
  cache.put("a", "updated"); // a becomes MRU, b becomes LRU
  cache.put("c", "3");       // evicts b (LRU)
  EXPECT_EQ(cache.get("b"), std::nullopt);
  EXPECT_EQ(cache.get("a"), "updated");
  EXPECT_EQ(cache.get("c"), "3");
}

TEST(CompLRUTest, DelNonexistentKeyReturnsFalse) {
  LRUCache cache(SMALL_CAPACITY);
  EXPECT_FALSE(cache.del("ghost"));
}

TEST(CompLRUTest, ConcurrentMixedReadWrite) {
  // Mixed put + get + del from multiple threads
  LRUCache cache(LARGE_CAPACITY);
  std::atomic<int> errors{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < 10; ++t) {
    threads.emplace_back([&cache, t, &errors]() {
      for (int i = 0; i < 1000; ++i) {
        std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
        cache.put(key, "val");
        auto val = cache.get(key);
        if (!val.has_value()) {
          ++errors;
        }
        // Delete half the keys
        if (i % 2 == 0) {
          (void)cache.del(key);
        }
      }
    });
  }

  for (auto &th : threads) {
    th.join();
  }
  EXPECT_EQ(errors.load(), 0);
}

TEST(CompLRUTest, LargeKeyAndValue) {
  LRUCache cache(10);
  std::string big_key(10000, 'K');
  std::string big_val(100000, 'V');
  cache.put(big_key, big_val);
  EXPECT_EQ(cache.get(big_key), big_val);
}

TEST(CompLRUTest, CapacityReturnsCorrectValue) {
  LRUCache cache(42);
  EXPECT_EQ(cache.capacity(), 42u);
}

// ===========================================================================
// Parser — Edge Cases
// ===========================================================================

TEST(CompParserTest, ExistsMissingKey) {
  auto cmd = CommandParser::parse("EXISTS");
  EXPECT_EQ(cmd.type, CommandType::EXISTS);
  EXPECT_FALSE(cmd.valid);
}

TEST(CompParserTest, TTLZeroSeconds) {
  // TTL with 0 seconds — parser accepts it as a valid non-negative integer.
  // The key will expire immediately, but the command itself is well-formed.
  auto cmd = CommandParser::parse("TTL mykey 0");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[1], "0");
}

TEST(CompParserTest, SetWithExtraArgs) {
  // SET key val extra tokens — extra tokens may be ignored or treated as value
  auto cmd = CommandParser::parse("SET key val extra tokens");
  EXPECT_EQ(cmd.type, CommandType::SET);
  // Depending on implementation: may be valid (extra ignored) or invalid
  // Our parser treats everything after key as a single value via tokenize
  EXPECT_TRUE(cmd.valid);
}

TEST(CompParserTest, TabSeparatedTokens) {
  auto cmd = CommandParser::parse("SET\tkey\tval");
  // Tab-separated should still be tokenized correctly
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_TRUE(cmd.valid);
}

TEST(CompParserTest, LeadingAndTrailingWhitespace) {
  auto cmd = CommandParser::parse("   SET  key  val   ");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "key");
}

TEST(CompParserTest, VeryLongInput) {
  // Parser should handle a very long input without crashing
  std::string long_key(10000, 'x');
  std::string input = "GET " + long_key;
  auto cmd = CommandParser::parse(input);
  EXPECT_EQ(cmd.type, CommandType::GET);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], long_key);
}

TEST(CompParserTest, NumericKey) {
  auto cmd = CommandParser::parse("SET 12345 67890");
  EXPECT_EQ(cmd.type, CommandType::SET);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[0], "12345");
  EXPECT_EQ(cmd.args[1], "67890");
}

TEST(CompParserTest, DelWithExtraArgs) {
  auto cmd = CommandParser::parse("DEL key extra");
  EXPECT_EQ(cmd.type, CommandType::DEL);
  // DEL only needs 1 key; extra args should be ignored or cause error
  EXPECT_TRUE(cmd.valid);
}

TEST(CompParserTest, TTLVeryLargeSeconds) {
  auto cmd = CommandParser::parse("TTL key 999999999");
  EXPECT_EQ(cmd.type, CommandType::TTL);
  EXPECT_TRUE(cmd.valid);
  EXPECT_EQ(cmd.args[1], "999999999");
}

// ===========================================================================
// TTL — Edge Cases & Concurrency
// ===========================================================================

TEST(CompTTLTest, ResetTTLExtensionsExpiry) {
  // Setting a new TTL should override the old one
  TTLManager ttl;
  ttl.set_expiry("key", 1);
  // Before the 1s TTL expires, extend it to 10s
  ttl.set_expiry("key", 10);
  // Wait past the original TTL
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));
  // Key should NOT be expired (new TTL of 10s hasn't passed)
  EXPECT_FALSE(ttl.is_expired("key"));
}

TEST(CompTTLTest, CleanupMultipleExpiredKeys) {
  LRUCache cache(MEDIUM_CAPACITY);
  TTLManager ttl;

  // Set 10 keys with 1s TTL
  for (int i = 0; i < 10; ++i) {
    std::string key = "expire" + std::to_string(i);
    cache.put(key, "val");
    ttl.set_expiry(key, SHORT_TTL_SECONDS);
  }
  // Set 5 keys with no TTL
  for (int i = 0; i < 5; ++i) {
    std::string key = "keep" + std::to_string(i);
    cache.put(key, "permanent");
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_SLEEP_MS));
  ttl.cleanup_expired(cache);

  // Expired keys gone
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(cache.get("expire" + std::to_string(i)), std::nullopt);
  }
  // Non-TTL keys remain
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(cache.get("keep" + std::to_string(i)), "permanent");
  }
}

TEST(CompTTLTest, ConcurrentSetExpiryAndCheck) {
  TTLManager ttl;
  std::atomic<int> errors{0};
  std::vector<std::thread> threads;

  // Writers set TTLs
  for (int t = 0; t < 5; ++t) {
    threads.emplace_back([&ttl, t]() {
      for (int i = 0; i < 100; ++i) {
        std::string key = "k_" + std::to_string(t) + "_" + std::to_string(i);
        ttl.set_expiry(key, 60); // 60s — won't expire during test
      }
    });
  }
  // Readers check expiry
  for (int t = 0; t < 5; ++t) {
    threads.emplace_back([&ttl, &errors]() {
      for (int i = 0; i < 100; ++i) {
        // Check a key that may or may not exist yet — should not crash
        (void)ttl.is_expired("k_0_" + std::to_string(i));
      }
    });
  }

  for (auto &th : threads) {
    th.join();
  }
  // No crashes, no data races (verified by ASan)
  SUCCEED();
}

TEST(CompTTLTest, RemoveNonExistentKeyNoOp) {
  TTLManager ttl;
  // Should not crash or throw
  ttl.remove("never_set");
  EXPECT_FALSE(ttl.is_expired("never_set"));
}

// ===========================================================================
// AOF — Edge Cases & Integration
// ===========================================================================

class CompAOFTest : public ::testing::Test {
protected:
  void SetUp() override { std::remove(COMP_AOF_PATH.c_str()); }
  void TearDown() override { std::remove(COMP_AOF_PATH.c_str()); }
};

TEST_F(CompAOFTest, ReplayMalformedLinesSkipsGracefully) {
  // AOF file contains some garbage/malformed lines
  {
    std::ofstream file(COMP_AOF_PATH);
    file << "SET name aman\n";
    file << "GARBAGE LINE\n";
    file << "SET city delhi\n";
    file << "INVALID\n";
  }

  // Replay — parse each line, skip invalid commands
  LRUCache cache(MEDIUM_CAPACITY);
  AOFReader reader(COMP_AOF_PATH);
  auto commands = reader.read_all_commands();

  int replayed = 0;
  for (const auto &cmd_str : commands) {
    Command cmd = CommandParser::parse(cmd_str);
    if (cmd.valid && cmd.type == CommandType::SET) {
      cache.put(cmd.args[0], cmd.args[1]);
      ++replayed;
    }
    // Invalid commands are silently skipped — no crash
  }

  EXPECT_EQ(replayed, 2);
  EXPECT_EQ(cache.get("name"), "aman");
  EXPECT_EQ(cache.get("city"), "delhi");
}

TEST_F(CompAOFTest, EmptyAOFFileReturnsNoCommands) {
  // Create an empty file
  { std::ofstream file(COMP_AOF_PATH); }

  AOFReader reader(COMP_AOF_PATH);
  auto commands = reader.read_all_commands();
  EXPECT_TRUE(commands.empty());
}

TEST_F(CompAOFTest, LargeReplayPerformance) {
  // Write 10,000 commands and replay — should complete quickly
  {
    AOFWriter writer(COMP_AOF_PATH);
    for (int i = 0; i < 10000; ++i) {
      writer.log_command("SET key" + std::to_string(i) + " val" +
                         std::to_string(i));
    }
  }

  LRUCache cache(LARGE_CAPACITY);
  AOFReader reader(COMP_AOF_PATH);
  auto commands = reader.read_all_commands();

  ASSERT_EQ(commands.size(), 10000u);

  for (const auto &cmd_str : commands) {
    Command cmd = CommandParser::parse(cmd_str);
    if (cmd.valid && cmd.type == CommandType::SET) {
      cache.put(cmd.args[0], cmd.args[1]);
    }
  }

  EXPECT_EQ(cache.size(), 10000u);
  EXPECT_EQ(cache.get("key0"), "val0");
  EXPECT_EQ(cache.get("key9999"), "val9999");
}

TEST_F(CompAOFTest, WriteAndReadEmptyCommand) {
  // Edge case: log_command with empty string
  {
    AOFWriter writer(COMP_AOF_PATH);
    writer.log_command("SET a b");
    writer.log_command(""); // empty command
    writer.log_command("SET c d");
  }

  AOFReader reader(COMP_AOF_PATH);
  auto commands = reader.read_all_commands();
  // Empty line should be filtered out by reader
  EXPECT_EQ(commands.size(), 2u);
}

// ===========================================================================
// Integration: Full Pipeline — Parser → Cache → TTL → AOF
// ===========================================================================

TEST_F(CompAOFTest, FullPipelineSetTTLDelReplay) {
  // Simulate a full server session: SET, TTL, DEL → write to AOF → replay
  {
    AOFWriter writer(COMP_AOF_PATH);
    writer.log_command("SET user alice");
    writer.log_command("SET session abc123");
    writer.log_command("TTL session 3600");
    writer.log_command("DEL user");
    writer.log_command("SET config maxconn");
  }

  // Replay into fresh cache
  LRUCache cache(MEDIUM_CAPACITY);
  TTLManager ttl;
  AOFReader reader(COMP_AOF_PATH);

  for (const auto &cmd_str : reader.read_all_commands()) {
    Command cmd = CommandParser::parse(cmd_str);
    if (!cmd.valid) {
      continue;
    }
    switch (cmd.type) {
    case CommandType::SET:
      cache.put(cmd.args[0], cmd.args[1]);
      break;
    case CommandType::DEL:
      ttl.remove(cmd.args[0]);
      (void)cache.del(cmd.args[0]);
      break;
    case CommandType::TTL:
      ttl.set_expiry(cmd.args[0], std::stoi(cmd.args[1]));
      break;
    default:
      break;
    }
  }

  // Verify state after replay
  EXPECT_EQ(cache.get("user"), std::nullopt); // DEL'd
  EXPECT_EQ(cache.get("session"), "abc123");  // still valid (TTL 3600s)
  EXPECT_FALSE(ttl.is_expired("session"));    // TTL hasn't expired
  EXPECT_EQ(cache.get("config"), "maxconn");  // present
  EXPECT_EQ(cache.size(), 2u);                // session + config
}

TEST(CompIntegrationTest, LazyExpiryOnGet) {
  // Simulate what Connection::execute_command does for GET with expired TTL
  LRUCache cache(MEDIUM_CAPACITY);
  TTLManager ttl;

  cache.put("temp", "value");
  ttl.set_expiry("temp", SHORT_TTL_SECONDS);

  // Before expiry: key accessible
  EXPECT_FALSE(ttl.is_expired("temp"));
  EXPECT_EQ(cache.get("temp"), "value");

  // Wait for expiry
  std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_SLEEP_MS));

  // Lazy expiry check (mimics Connection::execute_command GET path)
  EXPECT_TRUE(ttl.is_expired("temp"));
  (void)cache.del("temp");
  ttl.remove("temp");

  EXPECT_EQ(cache.get("temp"), std::nullopt);
  EXPECT_FALSE(ttl.is_expired("temp")); // removed from TTL map
}

TEST(CompIntegrationTest, LazyExpiryOnExists) {
  // Same pattern for EXISTS
  LRUCache cache(MEDIUM_CAPACITY);
  TTLManager ttl;

  cache.put("check", "val");
  ttl.set_expiry("check", SHORT_TTL_SECONDS);

  EXPECT_EQ(cache.get("check"), "val");

  std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_SLEEP_MS));

  // Lazy check: expired → delete → return not exists
  EXPECT_TRUE(ttl.is_expired("check"));
  (void)cache.del("check");
  ttl.remove("check");
  EXPECT_EQ(cache.get("check"), std::nullopt);
}

// ===========================================================================
// Stress: Mixed Operations Under Concurrency
// ===========================================================================

TEST(CompStressTest, ConcurrentLRUEvictionUnderLoad) {
  // Small cache capacity + many concurrent writers = constant eviction
  LRUCache cache(100);
  std::vector<std::thread> threads;

  for (int t = 0; t < 10; ++t) {
    threads.emplace_back([&cache, t]() {
      for (int i = 0; i < 1000; ++i) {
        std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
        cache.put(key, "val");
        // Some keys will have been evicted already — that's fine
        (void)cache.get(key);
      }
    });
  }

  for (auto &th : threads) {
    th.join();
  }

  // Cache should not exceed capacity
  EXPECT_LE(cache.size(), 100u);
  // And should have some items
  EXPECT_GT(cache.size(), 0u);
}

TEST(CompStressTest, ConcurrentTTLCleanupWithReads) {
  // One thread runs cleanup_expired while others read/write
  LRUCache cache(LARGE_CAPACITY);
  TTLManager ttl;
  std::atomic<bool> stop{false};

  // Populate with expiring keys
  for (int i = 0; i < 1000; ++i) {
    std::string key = "key" + std::to_string(i);
    cache.put(key, "val");
    ttl.set_expiry(key, SHORT_TTL_SECONDS);
  }

  // Cleanup thread
  std::thread cleaner([&]() {
    while (!stop.load()) {
      ttl.cleanup_expired(cache);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });

  // Reader threads
  std::vector<std::thread> readers;
  for (int t = 0; t < 4; ++t) {
    readers.emplace_back([&cache, &ttl]() {
      for (int i = 0; i < 1000; ++i) {
        std::string key = "key" + std::to_string(i % 1000);
        // Lazy check (mimics server path)
        if (ttl.is_expired(key)) {
          (void)cache.del(key);
          ttl.remove(key);
        } else {
          (void)cache.get(key);
        }
      }
    });
  }

  for (auto &th : readers) {
    th.join();
  }
  stop.store(true);
  cleaner.join();

  // No crashes or deadlocks — that's the assertion
  SUCCEED();
}
