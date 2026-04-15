#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "../src/core/lru.h"
#include "../src/core/ttl.h"

// ---------------------------------------------------------------------------
// Named constants (no magic numbers — .agent/common/coding-style.md)
// ---------------------------------------------------------------------------
constexpr int LONG_TTL_SECONDS = 10;
constexpr int SHORT_TTL_SECONDS = 1;
constexpr int SHORT_SLEEP_MS = 1100; // slightly > 1s to guarantee expiry
constexpr size_t TEST_CACHE_CAPACITY = 100;

// ===========================================================================
// TTLTest Suite
// ===========================================================================

TEST(TTLTest, SetExpiryAndCheckNotExpired) {
  // Arrange
  TTLManager ttl;
  const std::string key = "session:abc";

  // Act
  ttl.set_expiry(key, LONG_TTL_SECONDS);

  // Assert — key was just set, should NOT be expired
  EXPECT_FALSE(ttl.is_expired(key));
}

TEST(TTLTest, ExpiredKeyReturnsTrue) {
  // Arrange
  TTLManager ttl;
  const std::string key = "temp:xyz";
  ttl.set_expiry(key, SHORT_TTL_SECONDS);

  // Act — wait for expiry
  std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_SLEEP_MS));

  // Assert
  EXPECT_TRUE(ttl.is_expired(key));
}

TEST(TTLTest, NonExistentKeyNotExpired) {
  // Arrange
  TTLManager ttl;

  // Act & Assert — key never set, safe default = not expired
  EXPECT_FALSE(ttl.is_expired("ghost:key"));
}

TEST(TTLTest, RemoveDeletesExpiry) {
  // Arrange
  TTLManager ttl;
  const std::string key = "remove:me";
  ttl.set_expiry(key, SHORT_TTL_SECONDS);

  // Act
  ttl.remove(key);

  // Assert — removed key should behave as if no TTL was set
  EXPECT_FALSE(ttl.is_expired(key));
}

TEST(TTLTest, CleanupExpiredDeletesFromCache) {
  // Arrange
  LRUCache cache(TEST_CACHE_CAPACITY);
  TTLManager ttl;
  const std::string key = "expire:soon";
  cache.put(key, "value");
  ttl.set_expiry(key, SHORT_TTL_SECONDS);

  // Act — wait for expiry, then run cleanup
  std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_SLEEP_MS));
  ttl.cleanup_expired(cache);

  // Assert — key should be gone from cache AND from TTL map
  EXPECT_EQ(cache.get(key), std::nullopt);
  EXPECT_FALSE(ttl.is_expired(key));
}

TEST(TTLTest, CleanupPreservesUnexpired) {
  // Arrange
  LRUCache cache(TEST_CACHE_CAPACITY);
  TTLManager ttl;
  const std::string key = "keep:me";
  const std::string value = "important";
  cache.put(key, value);
  ttl.set_expiry(key, LONG_TTL_SECONDS);

  // Act
  ttl.cleanup_expired(cache);

  // Assert — key should still be accessible
  EXPECT_EQ(cache.get(key), value);
}
