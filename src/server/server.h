#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "../core/lru.h"
#include "../core/ttl.h"
#include "../parser/command.h"
#include "../persistence/aof.h"

class Server {
public:
  Server(int port, size_t cache_capacity);
  ~Server();

  void start();
  void stop();

  // Non-copyable, non-movable (owns socket + thread resources)
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

private:
  int port_;
  int server_fd_ = -1;
  std::atomic<bool> running_{false};
  LRUCache cache_;
  TTLManager ttl_;
  AOFWriter aof_;
  std::thread cleanup_thread_;

  void setup_socket();
  void handle_client(int client_fd);
  [[nodiscard]] std::string execute_command(const Command &cmd);
};
