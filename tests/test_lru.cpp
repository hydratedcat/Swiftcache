#include "../src/core/lru.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(LRUTest, PutAndGet) {
  // Arrange
  LRUCache cache(5);

  // Act
  cache.put("name", "aman");
  auto result = cache.get("name");

  // Assert
  EXPECT_EQ(result, "aman");
}

TEST(LRUTest, GetMissingKey) {
  // Arrange
  LRUCache cache(5);

  // Act
  auto result = cache.get("ghost");

  // Assert
  EXPECT_EQ(result, std::nullopt);
}

TEST(LRUTest, EvictsLeastRecentlyUsed) {
  // Arrange
  LRUCache cache(2);
  cache.put("a", "1");
  cache.put("b", "2");

  // Act: access "a" → "b" becomes LRU; insert "c" → evicts "b"
  cache.get("a");
  cache.put("c", "3");

  // Assert
  EXPECT_EQ(cache.get("b"), std::nullopt);
  EXPECT_EQ(cache.get("a"), "1");
  EXPECT_EQ(cache.get("c"), "3");
}

TEST(LRUTest, GetUpdatesRecency) {
  // Arrange
  LRUCache cache(2);
  cache.put("x", "10");
  cache.put("y", "20");

  // Act: access "x" → "y" becomes LRU; insert "z" → evicts "y"
  cache.get("x");
  cache.put("z", "30");

  // Assert
  EXPECT_EQ(cache.get("y"), std::nullopt);
  EXPECT_EQ(cache.get("x"), "10");
}

TEST(LRUTest, PutUpdatesExistingKey) {
  // Arrange
  LRUCache cache(2);
  cache.put("k", "old");

  // Act
  cache.put("k", "new");

  // Assert: size unchanged, value updated
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get("k"), "new");
}

TEST(LRUTest, DeleteKey) {
  // Arrange
  LRUCache cache(5);
  cache.put("foo", "bar");

  // Act
  bool first_del = cache.del("foo");
  bool second_del = cache.del("foo");

  // Assert
  EXPECT_TRUE(first_del);
  EXPECT_EQ(cache.get("foo"), std::nullopt);
  EXPECT_FALSE(second_del);
}

TEST(LRUTest, CapacityOne) {
  // Arrange
  LRUCache cache(1);
  cache.put("first", "1");

  // Act: second insert evicts first
  cache.put("second", "2");

  // Assert
  EXPECT_EQ(cache.get("first"), std::nullopt);
  EXPECT_EQ(cache.get("second"), "2");
}

TEST(LRUTest, O1Operations) {
  // Arrange
  LRUCache cache(1000000);

  // Act: insert 1M items
  for (int i = 0; i < 1000000; i++)
    cache.put(std::to_string(i), std::to_string(i));

  // Assert: all items present (performance sanity — must complete quickly)
  EXPECT_EQ(cache.size(), 1000000u);
}

TEST(LRUTest, ConcurrentPut) {
  // Arrange
  LRUCache cache(100000);
  std::vector<std::thread> threads;

  // Act: 10 threads × 1000 unique keys each
  for (int t = 0; t < 10; t++) {
    threads.emplace_back([&cache, t]() {
      for (int i = 0; i < 1000; i++)
        cache.put(std::to_string(t * 1000 + i), "val");
    });
  }
  for (auto &th : threads)
    th.join();

  // Assert: no data races, all 10,000 keys present
  EXPECT_EQ(cache.size(), 10000u);
}
