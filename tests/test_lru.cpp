#include <gtest/gtest.h>
#include "../src/core/lru.h"

TEST(LRUTest, Placeholder) {
    LRUCache cache(10);
    EXPECT_EQ(cache.capacity(), 10);
}
