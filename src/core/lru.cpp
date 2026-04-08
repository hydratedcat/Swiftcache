#include "lru.h"

LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {}
void LRUCache::put(const std::string& /*key*/, const std::string& /*value*/) {}
std::optional<std::string> LRUCache::get(const std::string& /*key*/) { return std::nullopt; }
bool LRUCache::del(const std::string& /*key*/) { return false; }
size_t LRUCache::size() const { return 0; }
size_t LRUCache::capacity() const { return capacity_; }
void LRUCache::evict_lru() {}
