#include "cache_service.h"

#include <string>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
CacheServiceImpl::CacheServiceImpl(LRUCache &cache, TTLManager &ttl,
                                   AOFWriter &aof)
    : cache_(cache), ttl_(ttl), aof_(aof) {}

// ---------------------------------------------------------------------------
// Set: put key/value → optionally set TTL → log to AOF
// ---------------------------------------------------------------------------
grpc::Status CacheServiceImpl::Set(grpc::ServerContext * /*ctx*/,
                                   const swiftcache::SetRequest *req,
                                   swiftcache::SetResponse *res) {
  cache_.put(req->key(), req->value());

  if (req->ttl_seconds() > 0) {
    ttl_.set_expiry(req->key(), req->ttl_seconds());
    // Log both SET and TTL commands to AOF for full replay
    aof_.log_command("SET " + req->key() + " " + req->value());
    aof_.log_command("TTL " + req->key() + " " +
                     std::to_string(req->ttl_seconds()));
  } else {
    aof_.log_command("SET " + req->key() + " " + req->value());
  }

  res->set_success(true);
  return grpc::Status::OK;
}

// ---------------------------------------------------------------------------
// Get: lazy TTL check → cache lookup → populate response
// ---------------------------------------------------------------------------
grpc::Status CacheServiceImpl::Get(grpc::ServerContext * /*ctx*/,
                                   const swiftcache::GetRequest *req,
                                   swiftcache::GetResponse *res) {
  const std::string &key = req->key();

  // Lazy TTL check — expired keys return miss
  if (ttl_.is_expired(key)) {
    (void)cache_.del(key);
    ttl_.remove(key);
    res->set_found(false);
    return grpc::Status::OK;
  }

  auto val = cache_.get(key);
  if (val.has_value()) {
    res->set_found(true);
    res->set_value(*val);
  } else {
    res->set_found(false);
  }

  return grpc::Status::OK;
}

// ---------------------------------------------------------------------------
// Del: remove TTL → delete from cache → log to AOF
// ---------------------------------------------------------------------------
grpc::Status CacheServiceImpl::Del(grpc::ServerContext * /*ctx*/,
                                   const swiftcache::DelRequest *req,
                                   swiftcache::DelResponse *res) {
  const std::string &key = req->key();
  ttl_.remove(key);
  bool deleted = cache_.del(key);
  res->set_deleted(deleted);

  if (deleted) {
    aof_.log_command("DEL " + key);
  }

  return grpc::Status::OK;
}

// ---------------------------------------------------------------------------
// Ping: health check — return "PONG"
// ---------------------------------------------------------------------------
grpc::Status CacheServiceImpl::Ping(grpc::ServerContext * /*ctx*/,
                                    const swiftcache::PingRequest * /*req*/,
                                    swiftcache::PingResponse *res) {
  res->set_message("PONG");
  return grpc::Status::OK;
}
