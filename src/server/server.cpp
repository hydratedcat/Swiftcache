#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// Named constants — no magic numbers
// ---------------------------------------------------------------------------
namespace {
constexpr size_t RECV_BUFFER_SIZE = 4096;
constexpr int TTL_CLEANUP_INTERVAL_SECONDS = 1;
constexpr int LISTEN_BACKLOG = SOMAXCONN;
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
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
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
// start: bind socket, launch TTL cleanup thread, enter accept loop
// ---------------------------------------------------------------------------
void Server::start() {
  setup_socket();
  running_ = true;

  std::cout << "SwiftCache listening on port " << port_ << "\n";

  // Background TTL cleanup sweep — runs every 1s
  cleanup_thread_ = std::thread([this]() {
    while (running_) {
      std::this_thread::sleep_for(
          std::chrono::seconds(TTL_CLEANUP_INTERVAL_SECONDS));
      ttl_.cleanup_expired(cache_);
    }
  });

  // Single-threaded accept loop (Day 7 upgrades to thread-per-client)
  while (running_) {
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (!running_) {
        break; // graceful shutdown
      }
      continue;
    }
    handle_client(client_fd);
    close(client_fd);
  }
}

// ---------------------------------------------------------------------------
// stop: flip flag → unblock accept → join cleanup thread
// ---------------------------------------------------------------------------
void Server::stop() {
  running_ = false;
  if (server_fd_ >= 0) {
    // shutdown() unblocks any thread blocked on accept()
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  if (cleanup_thread_.joinable()) {
    cleanup_thread_.join();
  }
}

// ---------------------------------------------------------------------------
// handle_client: read lines → parse → execute → send response
// ---------------------------------------------------------------------------
void Server::handle_client(int client_fd) {
  char buffer[RECV_BUFFER_SIZE];
  std::string leftover;

  while (running_) {
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
      break; // client disconnected or read error
    }

    buffer[bytes_read] = '\0';
    leftover += buffer;

    // Process all complete lines (delimited by \n or \r\n)
    std::string::size_type pos = 0;
    while ((pos = leftover.find('\n')) != std::string::npos) {
      std::string line = leftover.substr(0, pos);
      leftover.erase(0, pos + 1);

      // Strip trailing \r if present (handles \r\n from telnet/nc)
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line.empty()) {
        continue;
      }

      Command cmd = CommandParser::parse(line);
      std::string response = execute_command(cmd);

      // Log write commands to AOF (reads are never logged)
      if (cmd.valid && (cmd.type == CommandType::SET ||
                        cmd.type == CommandType::DEL ||
                        cmd.type == CommandType::TTL)) {
        aof_.log_command(line);
      }

      ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
      if (sent < 0) {
        return; // write error — drop this client
      }
    }
  }
}

// ---------------------------------------------------------------------------
// execute_command: dispatch to cache/ttl, return Redis-style response
// ---------------------------------------------------------------------------
//   +OK\r\n          — success
//   $<len>\r\n<val>\r\n — string value
//   $-1\r\n          — key not found
//   :<int>\r\n       — integer (EXISTS)
//   -ERR msg\r\n     — error
// ---------------------------------------------------------------------------
std::string Server::execute_command(const Command &cmd) {
  if (!cmd.valid) {
    return "-" + cmd.error_msg + "\r\n";
  }

  switch (cmd.type) {
  case CommandType::SET:
    cache_.put(cmd.args[0], cmd.args[1]);
    return "+OK\r\n";

  case CommandType::GET: {
    const std::string &key = cmd.args[0];
    // Lazy TTL check — expired keys return miss
    if (ttl_.is_expired(key)) {
      (void)cache_.del(key);
      ttl_.remove(key);
      return "$-1\r\n";
    }
    auto val = cache_.get(key);
    if (!val.has_value()) {
      return "$-1\r\n";
    }
    return "$" + std::to_string(val->size()) + "\r\n" + *val + "\r\n";
  }

  case CommandType::DEL: {
    const std::string &key = cmd.args[0];
    ttl_.remove(key);
    bool deleted = cache_.del(key);
    return deleted ? "+OK\r\n" : "$-1\r\n";
  }

  case CommandType::TTL: {
    const std::string &key = cmd.args[0];
    int seconds = std::stoi(cmd.args[1]);
    ttl_.set_expiry(key, seconds);
    return "+OK\r\n";
  }

  case CommandType::EXISTS: {
    const std::string &key = cmd.args[0];
    // Lazy TTL check before existence test
    if (ttl_.is_expired(key)) {
      (void)cache_.del(key);
      ttl_.remove(key);
      return ":0\r\n";
    }
    auto val = cache_.get(key);
    return val.has_value() ? ":1\r\n" : ":0\r\n";
  }

  case CommandType::PING:
    return "+PONG\r\n";

  case CommandType::UNKNOWN:
  default:
    return "-ERR unknown command\r\n";
  }
}
