#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../src/core/store.h"

// --- Basic Operations ---

TEST(StoreTest, SetAndGet) {
    Store store;
    store.set("name", "aman");
    EXPECT_EQ(store.get("name"), "aman");
}

TEST(StoreTest, GetMissingKey) {
    Store store;
    EXPECT_EQ(store.get("missing"), std::nullopt);
}

TEST(StoreTest, DeleteExistingKey) {
    Store store;
    store.set("key", "val");
    EXPECT_TRUE(store.del("key"));
    EXPECT_FALSE(store.exists("key"));
}

TEST(StoreTest, DeleteMissingKey) {
    Store store;
    EXPECT_FALSE(store.del("ghost"));
}

TEST(StoreTest, ExistsReturnsTrueForSetKey) {
    Store store;
    store.set("alive", "yes");
    EXPECT_TRUE(store.exists("alive"));
}

TEST(StoreTest, ExistsReturnsFalseForMissingKey) {
    Store store;
    EXPECT_FALSE(store.exists("nope"));
}

TEST(StoreTest, SizeReflectsInsertions) {
    Store store;
    EXPECT_EQ(store.size(), 0);
    store.set("a", "1");
    store.set("b", "2");
    EXPECT_EQ(store.size(), 2);
}

TEST(StoreTest, OverwriteExistingKey) {
    Store store;
    store.set("key", "old");
    store.set("key", "new");
    EXPECT_EQ(store.get("key"), "new");
    EXPECT_EQ(store.size(), 1);
}

TEST(StoreTest, DeleteDecreasesSize) {
    Store store;
    store.set("a", "1");
    store.set("b", "2");
    store.del("a");
    EXPECT_EQ(store.size(), 1);
}

// --- Concurrency ---

TEST(StoreTest, ConcurrentSetGet) {
    Store store;
    const int NUM_THREADS = 10;
    const int OPS_PER_THREAD = 1000;
    std::vector<std::thread> threads;

    // 10 writer threads, each writing 1000 unique keys
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                std::string key = std::to_string(t * OPS_PER_THREAD + i);
                store.set(key, "val");
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(store.size(), static_cast<size_t>(NUM_THREADS * OPS_PER_THREAD));

    // Verify all keys are readable
    for (int i = 0; i < NUM_THREADS * OPS_PER_THREAD; i++) {
        EXPECT_EQ(store.get(std::to_string(i)), "val");
    }
}
