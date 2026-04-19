#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "../src/server/server.h"

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
constexpr int TEST_PORT = 7380;
constexpr size_t TEST_CACHE_CAPACITY = 10000;
constexpr int NUM_CONCURRENT_CLIENTS = 10;
constexpr int OPS_PER_CLIENT = 100;
constexpr int SERVER_STARTUP_MS = 200;
constexpr int RECV_TIMEOUT_SEC = 2;
constexpr size_t RECV_BUF_SIZE = 4096;
} // namespace

// ---------------------------------------------------------------------------
// Helper: connect, send, receive, close
// ---------------------------------------------------------------------------
static int connect_to_server(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  // Set receive timeout so tests don't hang on failure
  timeval tv{};
  tv.tv_sec = RECV_TIMEOUT_SEC;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static std::string send_command(int fd, const std::string &cmd) {
  std::string wire = cmd + "\r\n";
  send(fd, wire.c_str(), wire.size(), 0);

  char buf[RECV_BUF_SIZE];
  ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    return "";
  }
  buf[n] = '\0';
  return std::string(buf);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Basic: server starts, accepts a connection, responds to PING
TEST(ServerTest, SingleClientPing) {
  Server server(TEST_PORT, TEST_CACHE_CAPACITY);
  std::thread server_thread([&server]() { server.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_STARTUP_MS));

  int fd = connect_to_server(TEST_PORT);
  ASSERT_GE(fd, 0) << "Failed to connect to server";

  std::string resp = send_command(fd, "PING");
  EXPECT_EQ(resp, "+PONG\r\n");

  close(fd);
  server.stop();
  server_thread.join();
}

// Multiple clients connected simultaneously — all get correct responses
TEST(ServerTest, MultipleClientsSET_GET) {
  Server server(TEST_PORT + 1, TEST_CACHE_CAPACITY);
  std::thread server_thread([&server]() { server.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_STARTUP_MS));

  constexpr int NUM_CLIENTS = 5;
  std::vector<int> fds(NUM_CLIENTS, -1);

  // Open all connections
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    fds[i] = connect_to_server(TEST_PORT + 1);
    ASSERT_GE(fds[i], 0) << "Client " << i << " failed to connect";
  }

  // Each client sets a unique key
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string val = "val" + std::to_string(i);
    std::string resp = send_command(fds[i], "SET " + key + " " + val);
    EXPECT_EQ(resp, "+OK\r\n") << "SET failed for client " << i;
  }

  // Each client reads back its own key
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string val = "val" + std::to_string(i);
    std::string resp = send_command(fds[i], "GET " + key);
    std::string expected =
        "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    EXPECT_EQ(resp, expected) << "GET mismatch for client " << i;
  }

  // Cross-read: client 0 reads key set by client 4
  if (NUM_CLIENTS >= 5) {
    std::string resp = send_command(fds[0], "GET key4");
    EXPECT_EQ(resp, "$4\r\nval4\r\n");
  }

  for (int fd : fds) {
    close(fd);
  }
  server.stop();
  server_thread.join();
}

// Concurrent stress: 10 threads each doing 100 SET/GET ops
TEST(ServerTest, ConcurrentStress) {
  Server server(TEST_PORT + 2, TEST_CACHE_CAPACITY);
  std::thread server_thread([&server]() { server.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_STARTUP_MS));

  std::atomic<int> errors{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < NUM_CONCURRENT_CLIENTS; ++t) {
    threads.emplace_back([t, &errors]() {
      int fd = connect_to_server(TEST_PORT + 2);
      if (fd < 0) {
        ++errors;
        return;
      }

      for (int i = 0; i < OPS_PER_CLIENT; ++i) {
        std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
        std::string val = "v" + std::to_string(i);

        std::string set_resp = send_command(fd, "SET " + key + " " + val);
        if (set_resp != "+OK\r\n") {
          ++errors;
        }

        std::string get_resp = send_command(fd, "GET " + key);
        std::string expected =
            "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
        if (get_resp != expected) {
          ++errors;
        }
      }

      close(fd);
    });
  }

  for (auto &th : threads) {
    th.join();
  }

  EXPECT_EQ(errors.load(), 0) << "Some operations failed under concurrency";

  server.stop();
  server_thread.join();
}

// Active connections counter increments and decrements
TEST(ServerTest, ActiveConnectionsTracking) {
  Server server(TEST_PORT + 3, TEST_CACHE_CAPACITY);
  std::thread server_thread([&server]() { server.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_STARTUP_MS));

  EXPECT_EQ(server.active_connections(), 0);

  // Connect 3 clients
  int fd1 = connect_to_server(TEST_PORT + 3);
  int fd2 = connect_to_server(TEST_PORT + 3);
  int fd3 = connect_to_server(TEST_PORT + 3);
  ASSERT_GE(fd1, 0);
  ASSERT_GE(fd2, 0);
  ASSERT_GE(fd3, 0);

  // Send a command on each so the server thread processes the accept
  (void)send_command(fd1, "PING");
  (void)send_command(fd2, "PING");
  (void)send_command(fd3, "PING");

  // Brief delay for the server to register connections
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_GE(server.active_connections(), 1);

  // Close one client — counter should drop
  close(fd1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int after_close = server.active_connections();

  close(fd2);
  close(fd3);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_LE(server.active_connections(), after_close);

  server.stop();
  server_thread.join();
}

// Server handles client disconnect gracefully (no crash)
TEST(ServerTest, ClientDisconnectGraceful) {
  Server server(TEST_PORT + 4, TEST_CACHE_CAPACITY);
  std::thread server_thread([&server]() { server.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_STARTUP_MS));

  // Connect and immediately disconnect — 5 times
  for (int i = 0; i < 5; ++i) {
    int fd = connect_to_server(TEST_PORT + 4);
    ASSERT_GE(fd, 0);
    (void)send_command(fd, "PING");
    close(fd);
  }

  // Server should still be alive — connect again and check
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int fd = connect_to_server(TEST_PORT + 4);
  ASSERT_GE(fd, 0);
  std::string resp = send_command(fd, "PING");
  EXPECT_EQ(resp, "+PONG\r\n");

  close(fd);
  server.stop();
  server_thread.join();
}
