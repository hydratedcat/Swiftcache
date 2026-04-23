#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>

#include "../src/core/lru.h"
#include "../src/core/ttl.h"
#include "../src/grpc/cache_service.h"
#include "../src/persistence/aof.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
constexpr size_t TEST_CACHE_CAPACITY = 1000;
constexpr const char *TEST_AOF_PATH = "test_grpc.aof";
constexpr int CONCURRENT_THREADS = 10;
constexpr int OPS_PER_THREAD = 100;
} // namespace

// ---------------------------------------------------------------------------
// Test fixture: spins up an in-process gRPC server per test
// ---------------------------------------------------------------------------
class GrpcTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Clean AOF from previous runs
    std::remove(TEST_AOF_PATH);

    cache_ = std::make_unique<LRUCache>(TEST_CACHE_CAPACITY);
    ttl_ = std::make_unique<TTLManager>();
    aof_ = std::make_unique<AOFWriter>(TEST_AOF_PATH);
    service_ =
        std::make_unique<CacheServiceImpl>(*cache_, *ttl_, *aof_);

    // Build in-process gRPC server on a random port
    grpc::ServerBuilder builder;
    int selected_port = 0;
    builder.AddListeningPort("localhost:0",
                             grpc::InsecureServerCredentials(),
                             &selected_port);
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    ASSERT_GT(selected_port, 0);

    // Create a channel to the in-process server
    auto channel = grpc::CreateChannel(
        "localhost:" + std::to_string(selected_port),
        grpc::InsecureChannelCredentials());
    stub_ = swiftcache::CacheService::NewStub(channel);
  }

  void TearDown() override {
    server_->Shutdown();
    server_.reset();
    service_.reset();
    aof_.reset();
    ttl_.reset();
    cache_.reset();
    std::remove(TEST_AOF_PATH);
  }

  std::unique_ptr<LRUCache> cache_;
  std::unique_ptr<TTLManager> ttl_;
  std::unique_ptr<AOFWriter> aof_;
  std::unique_ptr<CacheServiceImpl> service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<swiftcache::CacheService::Stub> stub_;
};

// ---------------------------------------------------------------------------
// Test: Ping returns "PONG"
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, PingReturnsMessage) {
  // Arrange
  grpc::ClientContext ctx;
  swiftcache::PingRequest req;
  swiftcache::PingResponse res;

  // Act
  grpc::Status status = stub_->Ping(&ctx, req, &res);

  // Assert
  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_EQ(res.message(), "PONG");
}

// ---------------------------------------------------------------------------
// Test: Set then Get returns correct value
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, SetAndGetKey) {
  // Arrange — Set a key
  {
    grpc::ClientContext ctx;
    swiftcache::SetRequest req;
    req.set_key("name");
    req.set_value("aman");
    swiftcache::SetResponse res;

    grpc::Status status = stub_->Set(&ctx, req, &res);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_TRUE(res.success());
  }

  // Act — Get the key
  grpc::ClientContext ctx;
  swiftcache::GetRequest req;
  req.set_key("name");
  swiftcache::GetResponse res;

  grpc::Status status = stub_->Get(&ctx, req, &res);

  // Assert
  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_TRUE(res.found());
  EXPECT_EQ(res.value(), "aman");
}

// ---------------------------------------------------------------------------
// Test: Get non-existent key returns found=false
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, GetMissingKey) {
  // Arrange — no keys set
  grpc::ClientContext ctx;
  swiftcache::GetRequest req;
  req.set_key("nonexistent");
  swiftcache::GetResponse res;

  // Act
  grpc::Status status = stub_->Get(&ctx, req, &res);

  // Assert
  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(res.found());
}

// ---------------------------------------------------------------------------
// Test: Set → Del → Get returns found=false
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, DeleteKey) {
  // Arrange — Set a key
  {
    grpc::ClientContext ctx;
    swiftcache::SetRequest req;
    req.set_key("city");
    req.set_value("delhi");
    swiftcache::SetResponse res;
    (void)stub_->Set(&ctx, req, &res);
  }

  // Act — Delete the key
  {
    grpc::ClientContext ctx;
    swiftcache::DelRequest req;
    req.set_key("city");
    swiftcache::DelResponse res;

    grpc::Status status = stub_->Del(&ctx, req, &res);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_TRUE(res.deleted());
  }

  // Assert — Get should return not found
  grpc::ClientContext ctx;
  swiftcache::GetRequest req;
  req.set_key("city");
  swiftcache::GetResponse res;

  grpc::Status status = stub_->Get(&ctx, req, &res);
  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(res.found());
}

// ---------------------------------------------------------------------------
// Test: Set with TTL → wait → Get returns found=false (expired)
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, SetWithTTLExpires) {
  // Arrange — Set a key with 1-second TTL
  {
    grpc::ClientContext ctx;
    swiftcache::SetRequest req;
    req.set_key("temp");
    req.set_value("ephemeral");
    req.set_ttl_seconds(1);
    swiftcache::SetResponse res;

    grpc::Status status = stub_->Set(&ctx, req, &res);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_TRUE(res.success());
  }

  // Verify key exists before TTL expires
  {
    grpc::ClientContext ctx;
    swiftcache::GetRequest req;
    req.set_key("temp");
    swiftcache::GetResponse res;

    grpc::Status status = stub_->Get(&ctx, req, &res);
    ASSERT_TRUE(status.ok());
    EXPECT_TRUE(res.found());
    EXPECT_EQ(res.value(), "ephemeral");
  }

  // Act — Wait for TTL to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // Assert — Key should be expired
  grpc::ClientContext ctx;
  swiftcache::GetRequest req;
  req.set_key("temp");
  swiftcache::GetResponse res;

  grpc::Status status = stub_->Get(&ctx, req, &res);
  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(res.found());
}

// ---------------------------------------------------------------------------
// Test: Concurrent Set/Get from multiple threads — no crashes/races
// ---------------------------------------------------------------------------
TEST_F(GrpcTest, ConcurrentGrpcOps) {
  // Arrange
  std::vector<std::thread> threads;

  // Act — spawn threads doing Set + Get
  for (int t = 0; t < CONCURRENT_THREADS; ++t) {
    threads.emplace_back([this, t]() {
      for (int i = 0; i < OPS_PER_THREAD; ++i) {
        std::string key =
            "key_" + std::to_string(t) + "_" + std::to_string(i);
        std::string value = "val_" + std::to_string(i);

        // Set
        {
          grpc::ClientContext ctx;
          swiftcache::SetRequest req;
          req.set_key(key);
          req.set_value(value);
          swiftcache::SetResponse res;
          grpc::Status status = stub_->Set(&ctx, req, &res);
          EXPECT_TRUE(status.ok()) << status.error_message();
        }

        // Get
        {
          grpc::ClientContext ctx;
          swiftcache::GetRequest req;
          req.set_key(key);
          swiftcache::GetResponse res;
          grpc::Status status = stub_->Get(&ctx, req, &res);
          EXPECT_TRUE(status.ok()) << status.error_message();
          EXPECT_TRUE(res.found());
          EXPECT_EQ(res.value(), value);
        }
      }
    });
  }

  for (auto &th : threads) {
    th.join();
  }

  // Assert — all keys should be accessible
  EXPECT_GE(cache_->size(), 1u);
}
