#include "server.h"

#include "connection.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// Named constants — no magic numbers
// ---------------------------------------------------------------------------
namespace {
constexpr int TTL_CLEANUP_INTERVAL_SECONDS = 1;
constexpr int LISTEN_BACKLOG = SOMAXCONN;
constexpr int SHUTDOWN_DRAIN_POLL_MS = 50;
constexpr int SHUTDOWN_DRAIN_TIMEOUT_MS = 5000;
} // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
Server::Server(int port, size_t cache_capacity)
    : port_(port), cache_(cache_capacity), aof_("swiftcache.aof") {}

Server::~Server() { stop(); }

// ---------------------------------------------------------------------------
// setup_socket: create → SO_REUSEADDR → bind → listen
// ---------------------------------------------------------------------------
void Server::setup_socket() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  // Allow immediate restart after crash — skip TIME_WAIT
  int opt = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    close(server_fd_);
    throw std::runtime_error("setsockopt SO_REUSEADDR failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port_));

  if (bind(server_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close(server_fd_);
    throw std::runtime_error("bind failed on port " + std::to_string(port_));
  }

  if (listen(server_fd_, LISTEN_BACKLOG) < 0) {
    close(server_fd_);
    throw std::runtime_error("listen failed");
  }
}

// ---------------------------------------------------------------------------
// start: bind socket, launch TTL cleanup thread, multi-threaded accept loop
// ---------------------------------------------------------------------------
void Server::start() {
  setup_socket();
  running_ = true;

  // Replay AOF log to restore cache state from previous session
  replay_aof();

  std::cout << "SwiftCache listening on port " << port_ << "\n";

  // Background TTL cleanup sweep — runs every 1s
  cleanup_thread_ = std::thread([this]() {
    while (running_) {
      std::this_thread::sleep_for(
          std::chrono::seconds(TTL_CLEANUP_INTERVAL_SECONDS));
      ttl_.cleanup_expired(cache_);
    }
  });

  // Multi-threaded accept loop: spawn one detached thread per client
  while (running_) {
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (!running_) {
        break; // graceful shutdown
      }
      continue;
    }

    // Each client gets its own thread; the Connection object tracks
    // active_connections_ via RAII-style increment/decrement.
    std::thread([this, client_fd]() {
      Connection conn(client_fd, cache_, ttl_, aof_, running_,
                      active_connections_);
      conn.handle();
    }).detach();
  }
}

// ---------------------------------------------------------------------------
// stop: flip flag → unblock accept → drain active clients → join cleanup
// ---------------------------------------------------------------------------
void Server::stop() {
  running_ = false;

  if (server_fd_ >= 0) {
    // shutdown() unblocks any thread blocked on accept()
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }

  // Wait for active client threads to finish (bounded drain)
  int waited_ms = 0;
  while (active_connections_ > 0 && waited_ms < SHUTDOWN_DRAIN_TIMEOUT_MS) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(SHUTDOWN_DRAIN_POLL_MS));
    waited_ms += SHUTDOWN_DRAIN_POLL_MS;
  }

  if (active_connections_ > 0) {
    std::cerr << "Warning: " << active_connections_.load()
              << " client(s) still active after shutdown timeout\n";
  }

  if (cleanup_thread_.joinable()) {
    cleanup_thread_.join();
  }
}

// ---------------------------------------------------------------------------
// active_connections: public accessor for connection counter
// ---------------------------------------------------------------------------
int Server::active_connections() const { return active_connections_.load(); }

// ---------------------------------------------------------------------------
// replay_aof: read AOF log → parse → execute directly (skip re-logging)
// ---------------------------------------------------------------------------
void Server::replay_aof() {
  AOFReader reader(aof_.filepath());
  std::vector<std::string> commands = reader.read_all_commands();

  if (commands.empty()) {
    return;
  }

  int replayed = 0;
  for (const auto &cmd_str : commands) {
    Command cmd = CommandParser::parse(cmd_str);
    if (!cmd.valid) {
      continue; // skip malformed lines
    }

    // Execute write commands directly against cache/TTL
    switch (cmd.type) {
    case CommandType::SET:
      cache_.put(cmd.args[0], cmd.args[1]);
      ++replayed;
      break;
    case CommandType::DEL:
      ttl_.remove(cmd.args[0]);
      (void)cache_.del(cmd.args[0]);
      ++replayed;
      break;
    case CommandType::TTL: {
      int seconds = std::stoi(cmd.args[1]);
      ttl_.set_expiry(cmd.args[0], seconds);
      ++replayed;
      break;
    }
    default:
      break; // ignore read-only commands in log
    }
  }

  std::cout << "AOF: replayed " << replayed << " command(s) from "
            << aof_.filepath() << "\n";
}

