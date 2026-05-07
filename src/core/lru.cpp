#include "lru.h"

LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {}

void LRUCache::put(const std::string &key, const std::string &value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = cache_map_.find(key);
  if (it != cache_map_.end()) {
    // Key exists: update in-place, splice to front (O(1), no copy)
    it->second->second = value;
    cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
    return;
  }
  if (cache_list_.size() >= capacity_) {
    evict_lru(); // remove back (LRU) before inserting new key
  }
  cache_list_.emplace_front(key, value);
  cache_map_[key] = cache_list_.begin();
}

std::optional<std::string> LRUCache::get(const std::string &key) {
  // Exclusive lock required: splice() mutates the list (LRU promotion)
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = cache_map_.find(key);
  if (it == cache_map_.end()) {
    return std::nullopt;
  }
  // Promote to front — marks as most recently used
  cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
  return it->second->second;
}

bool LRUCache::del(const std::string &key) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = cache_map_.find(key);
  if (it == cache_map_.end()) {
    return false;
  }
  cache_list_.erase(it->second);
  cache_map_.erase(it);
  return true;
}

size_t LRUCache::size() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return cache_list_.size();
}

size_t LRUCache::capacity() const { return capacity_; }

// Caller must hold mutex_. Removes tail (LRU) from both structures.
void LRUCache::evict_lru() {
  auto &[lru_key, lru_val] = cache_list_.back(); // C++17 structured binding
  cache_map_.erase(lru_key);
  cache_list_.pop_back();
}
