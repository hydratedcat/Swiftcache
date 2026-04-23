#include "server.h"

#include <csignal>
#include <iostream>

namespace {
constexpr int DEFAULT_PORT = 6380;
constexpr int DEFAULT_GRPC_PORT = 6381;
constexpr size_t DEFAULT_CACHE_CAPACITY = 10000;

// Global pointer for signal handler — the only way POSIX signals can reach
// the Server instance.  Lifetime is guaranteed: main() owns the Server object
// on the stack and it outlives any possible signal delivery.
Server *g_server = nullptr;
} // namespace

void signal_handler(int /*signum*/) {
  if (g_server != nullptr) {
    g_server->stop();
  }
}

int main() {
  Server server(DEFAULT_PORT, DEFAULT_CACHE_CAPACITY, DEFAULT_GRPC_PORT);
  g_server = &server;

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "SwiftCache v1.0 — starting...\n";

  try {
    server.start();
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
