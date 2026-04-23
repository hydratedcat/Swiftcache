#pragma once
#include <grpcpp/grpcpp.h>

#include "cache.grpc.pb.h"
#include "../core/lru.h"
#include "../core/ttl.h"
#include "../persistence/aof.h"

/// gRPC service implementation that delegates to the shared LRUCache,
/// TTLManager, and AOFWriter — the same instances the TCP server uses.
class CacheServiceImpl final : public swiftcache::CacheService::Service {
public:
  CacheServiceImpl(LRUCache &cache, TTLManager &ttl, AOFWriter &aof);

  grpc::Status Set(grpc::ServerContext *ctx,
                   const swiftcache::SetRequest *req,
                   swiftcache::SetResponse *res) override;

  grpc::Status Get(grpc::ServerContext *ctx,
                   const swiftcache::GetRequest *req,
                   swiftcache::GetResponse *res) override;

  grpc::Status Del(grpc::ServerContext *ctx,
                   const swiftcache::DelRequest *req,
                   swiftcache::DelResponse *res) override;

  grpc::Status Ping(grpc::ServerContext *ctx,
                    const swiftcache::PingRequest *req,
                    swiftcache::PingResponse *res) override;

  // Non-copyable, non-movable (holds references to shared state)
  CacheServiceImpl(const CacheServiceImpl &) = delete;
  CacheServiceImpl &operator=(const CacheServiceImpl &) = delete;
  CacheServiceImpl(CacheServiceImpl &&) = delete;
  CacheServiceImpl &operator=(CacheServiceImpl &&) = delete;

private:
  LRUCache &cache_;
  TTLManager &ttl_;
  AOFWriter &aof_;
};
