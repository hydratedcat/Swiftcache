#include "store.h"

void Store::set(const std::string &key, const std::string &value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  data_[key] = value;
}

std::optional<std::string> Store::get(const std::string &key) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = data_.find(key);
  if (it == data_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool Store::del(const std::string &key) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  return data_.erase(key) > 0;
}

bool Store::exists(const std::string &key) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return data_.find(key) != data_.end();
}

size_t Store::size() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return data_.size();
}
