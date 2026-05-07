#pragma once
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class LRUCache {
public:
  explicit LRUCache(size_t capacity);
  void put(const std::string &key, const std::string &value);
  [[nodiscard]] std::optional<std::string> get(const std::string &key);
  [[nodiscard]] bool del(const std::string &key);
  [[nodiscard]] size_t size() const;
  [[nodiscard]] size_t capacity() const;

private:
  size_t capacity_;
  std::list<std::pair<std::string, std::string>> cache_list_;
  std::unordered_map<std::string,
                     std::list<std::pair<std::string, std::string>>::iterator>
      cache_map_;
  mutable std::shared_mutex mutex_;

  void evict_lru();
};
