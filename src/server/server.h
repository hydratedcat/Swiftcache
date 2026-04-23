#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "../core/lru.h"
#include "../core/ttl.h"
#include "../grpc/cache_service.h"
#include "../parser/command.h"
#include "../persistence/aof.h"

class Server {
public:
  Server(int port, size_t cache_capacity, int grpc_port = 0);
  ~Server();

  void start();
  void stop();

  /// Returns the number of currently connected TCP clients.
  [[nodiscard]] int active_connections() const;

  /// Returns the gRPC port the server is listening on (0 if disabled).
  [[nodiscard]] int grpc_port() const;

  // Non-copyable, non-movable (owns socket + thread resources)
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

private:
  int port_;
  int grpc_port_;
  int server_fd_ = -1;
  std::atomic<bool> running_{false};
  std::atomic<int> active_connections_{0};
  LRUCache cache_;
  TTLManager ttl_;
  AOFWriter aof_;
  std::thread cleanup_thread_;

  // gRPC resources
  std::unique_ptr<CacheServiceImpl> grpc_service_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::thread grpc_thread_;

  void setup_socket();

  /// Replays the AOF log to restore cache state from disk.
  void replay_aof();

  /// Starts the gRPC server on grpc_port_ in a background thread.
  void start_grpc();

  /// Gracefully stops the gRPC server and joins its thread.
  void stop_grpc();
};
