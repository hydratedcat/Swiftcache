#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

// Forward declaration
class LRUCache;

class TTLManager {
public:
  void set_expiry(const std::string &key, int seconds);
  bool is_expired(const std::string &key) const;
  void remove(const std::string &key);
  void cleanup_expired(LRUCache &cache);

private:
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      expiry_map_;
  mutable std::mutex mutex_;
};
