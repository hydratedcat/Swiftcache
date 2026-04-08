#include <gtest/gtest.h>
#include "../src/core/store.h"

TEST(StoreTest, Placeholder) {
    Store store;
    EXPECT_EQ(store.size(), 0);
}
