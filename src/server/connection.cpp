#include "connection.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
constexpr size_t RECV_BUFFER_SIZE = 4096;
} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Connection::Connection(int client_fd, LRUCache &cache, TTLManager &ttl,
                       AOFWriter &aof, std::atomic<bool> &running,
                       std::atomic<int> &active_connections)
    : client_fd_(client_fd), cache_(cache), ttl_(ttl), aof_(aof),
      running_(running), active_connections_(active_connections) {
  ++active_connections_;
}

// ---------------------------------------------------------------------------
// handle: read lines → parse → execute → send response
// ---------------------------------------------------------------------------
void Connection::handle() {
  char buffer[RECV_BUFFER_SIZE];
  std::string leftover;

  while (running_) {
    ssize_t bytes_read = recv(client_fd_, buffer, sizeof(buffer) - 1, 0);
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
      if (cmd.valid &&
          (cmd.type == CommandType::SET || cmd.type == CommandType::DEL ||
           cmd.type == CommandType::TTL)) {
        aof_.log_command(line);
      }

      ssize_t sent = send(client_fd_, response.c_str(), response.size(), 0);
      if (sent < 0) {
        break; // write error — drop this client
      }
    }
  }

  close(client_fd_);
  --active_connections_;
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
std::string Connection::execute_command(const Command &cmd) {
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
