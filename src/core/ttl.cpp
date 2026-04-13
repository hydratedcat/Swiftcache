#include "ttl.h"
#include "lru.h"

void TTLManager::set_expiry(const std::string & /*key*/, int /*seconds*/) {}
bool TTLManager::is_expired(const std::string & /*key*/) const { return false; }
void TTLManager::remove(const std::string & /*key*/) {}
void TTLManager::cleanup_expired(LRUCache & /*cache*/) {}
