#pragma once
#include <atomic>
#include <string>

#include "../core/lru.h"
#include "../core/ttl.h"
#include "../parser/command.h"
#include "../persistence/aof.h"

// Per-client connection handler.  Each instance is created by the Server
// accept loop, runs on its own std::thread, and self-destructs when the
// client disconnects.  All shared state (cache, ttl, aof, counter) is
// accessed through references — ownership stays with Server.
class Connection {
public:
  Connection(int client_fd, LRUCache &cache, TTLManager &ttl, AOFWriter &aof,
             std::atomic<bool> &running, std::atomic<int> &active_connections);

  // Run the read→parse→execute→respond loop until client disconnects.
  void handle();

  // Non-copyable, non-movable (bound to a specific client fd)
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&) = delete;
  Connection &operator=(Connection &&) = delete;

private:
  int client_fd_;
  LRUCache &cache_;
  TTLManager &ttl_;
  AOFWriter &aof_;
  std::atomic<bool> &running_;
  std::atomic<int> &active_connections_;

  [[nodiscard]] std::string execute_command(const Command &cmd);
};
