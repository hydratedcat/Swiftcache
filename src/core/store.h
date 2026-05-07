#pragma once
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Store {
public:
  void set(const std::string &key, const std::string &value);
  [[nodiscard]] std::optional<std::string> get(const std::string &key);
  [[nodiscard]] bool del(const std::string &key);
  [[nodiscard]] bool exists(const std::string &key);
  [[nodiscard]] size_t size() const;

private:
  std::unordered_map<std::string, std::string> data_;
  mutable std::shared_mutex mutex_;
};
