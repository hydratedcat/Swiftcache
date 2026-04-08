#pragma once
#include <list>
#include <unordered_map>
#include <string>
#include <mutex>
#include <optional>

class LRUCache {
public:
    explicit LRUCache(size_t capacity);
    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    size_t size() const;
    size_t capacity() const;

private:
    size_t capacity_;
    std::list<std::pair<std::string, std::string>> cache_list_;
    std::unordered_map<std::string,
        std::list<std::pair<std::string, std::string>>::iterator> cache_map_;
    mutable std::mutex mutex_;

    void evict_lru();
};
