#include "ttl.h"
#include "lru.h"

#include <vector>

void TTLManager::set_expiry(const std::string &key, int seconds) {
  std::lock_guard<std::mutex> lock(mutex_);
  expiry_map_[key] =
      std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

bool TTLManager::is_expired(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = expiry_map_.find(key);
  if (it == expiry_map_.end()) {
    return false; // no TTL set → never expires
  }
  return std::chrono::steady_clock::now() > it->second;
}

void TTLManager::remove(const std::string &key) {
  std::lock_guard<std::mutex> lock(mutex_);
  expiry_map_.erase(key);
}

void TTLManager::cleanup_expired(LRUCache &cache) {
  // Collect expired keys under lock, then release before calling cache.del()
  // to avoid lock-order inversion (LRUCache has its own mutex).
  std::vector<std::string> expired_keys;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    for (const auto &[key, expiry_time] : expiry_map_) {
      if (now > expiry_time) {
        expired_keys.push_back(key);
      }
    }
    // Erase from TTL map while still holding the lock
    for (const auto &key : expired_keys) {
      expiry_map_.erase(key);
    }
  }
  // Delete from cache WITHOUT holding TTL mutex
  for (const auto &key : expired_keys) {
    (void)cache.del(key);
  }
}
