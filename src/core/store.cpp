#include "store.h"

void Store::set(const std::string& /*key*/, const std::string& /*value*/) {}
std::optional<std::string> Store::get(const std::string& /*key*/) { return std::nullopt; }
bool Store::del(const std::string& /*key*/) { return false; }
bool Store::exists(const std::string& /*key*/) { return false; }
size_t Store::size() const { return 0; }
