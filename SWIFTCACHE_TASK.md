# SwiftCache — Build Task List

## AI Assistant Directives (Strict Token & Learning Optimization)

To maintain a lean context window and ensure maximum learning, the AI must strictly follow these rules for every interaction:

1. **The Silent Logger:** Do not output long explanations in the chat. Create and maintain a `study-guide.md` file in the root. Before writing any code, silently log the specific Data Structure/Algorithm chosen, the "why", the Time/Space (Big O) complexity, and explanations of any terminal commands into that file.
2. **Diff-Only Output:** Never output entire files when making modifications. Only output the exact lines being changed, surrounded by a few lines of context (standard diff format).
3. **Pseudo-Code First:** Before writing raw C++ for a new algorithm, provide a maximum 5-line pseudo-code summary in the chat. Wait for my confirmation before generating the actual C++.
4. **Targeted Reading:** Do not read the entire `.agent/` directory at once. Only read a specific C++ standard file (e.g., `.agent/common/testing.md`) immediately before performing a relevant task.


## Project Overview
A Redis-like in-memory key-value store built from scratch in C++.
Demonstrates pure systems engineering — TCP server, LRU eviction,
AOF persistence, gRPC interface, multithreading, and benchmarking.

**Why this project matters:** Any backend/systems interviewer can ask you
to explain exactly how Redis works internally. After building this,
you'll know — because you built it yourself.

**End goal:** Working KV store + gRPC interface + benchmark results
showing ~2x throughput vs single-threaded baseline. Clean GitHub repo
with README, architecture diagram, and benchmark screenshots.

---

## Tech Stack
- **Language:** C++17
- **Build system:** CMake
- **Networking:** POSIX sockets (TCP)
- **RPC:** gRPC + Protocol Buffers
- **Threading:** std::thread, std::mutex, std::condition_variable
- **Persistence:** AOF (Append-Only File) — custom implementation
- **Benchmarking:** Google Benchmark
- **Testing:** Google Test (gtest)
- **Python client:** Python socket library (pure stdlib, no deps)

---

## Install Everything Before Starting

### Linux / WSL (recommended)
```bash
# Build tools
sudo apt update
sudo apt install -y build-essential cmake git pkg-config

# gRPC + Protobuf
sudo apt install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# Google Test + Google Benchmark
sudo apt install -y libgtest-dev libbenchmark-dev

# Python (for client library)
sudo apt install -y python3 python3-pip
pip3 install grpcio grpcio-tools
```

### Verify installs
```bash
cmake --version      # should be 3.15+
g++ --version        # should be 9+
protoc --version     # should be 3.x
python3 --version    # should be 3.8+
```

---

## Folder Structure — Set Up on Day 1

```
SwiftCache/
├── src/
│   ├── server/
│   │   ├── main.cpp            ← TCP server entry point
│   │   ├── server.cpp/.h       ← TCP server logic
│   │   └── connection.cpp/.h   ← per-client connection handler
│   ├── core/
│   │   ├── store.cpp/.h        ← hash map (thread-safe KV store)
│   │   ├── lru.cpp/.h          ← LRU eviction policy
│   │   └── ttl.cpp/.h          ← TTL expiry manager
│   ├── persistence/
│   │   └── aof.cpp/.h          ← Append-Only File
│   ├── parser/
│   │   └── command.cpp/.h      ← command parser (SET/GET/DEL/TTL)
│   └── grpc/
│       ├── cache_service.cpp/.h ← gRPC service implementation
│       └── cache.proto          ← protobuf definition
├── tests/
│   ├── test_store.cpp
│   ├── test_lru.cpp
│   ├── test_ttl.cpp
│   ├── test_parser.cpp
│   └── test_aof.cpp
├── benchmarks/
│   └── bench_store.cpp
├── client/
│   ├── client.py               ← Python TCP client
│   └── grpc_client.py          ← Python gRPC client
├── proto/
│   └── cache.proto
├── CMakeLists.txt
├── .github/
│   └── workflows/
│       └── ci.yml
└── README.md
```

---

## Week 1 — Core KV Store + TCP Server (Days 1–7)

### Day 1 — Project Setup + CMake
- [ ] Create folder structure above
- [ ] Write `CMakeLists.txt`:
  ```cmake
  cmake_minimum_required(VERSION 3.15)
  project(SwiftCache VERSION 1.0)

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
  set(CMAKE_BUILD_TYPE Release)

  # Add compiler optimisation flags
  add_compile_options(-O2 -Wall -Wextra -pthread)

  # Main server executable
  add_executable(swiftcache
      src/server/main.cpp
      src/server/server.cpp
      src/core/store.cpp
      src/core/lru.cpp
      src/core/ttl.cpp
      src/parser/command.cpp
      src/persistence/aof.cpp
  )
  target_link_libraries(swiftcache pthread)

  # Tests
  enable_testing()
  find_package(GTest REQUIRED)
  add_executable(run_tests
      tests/test_store.cpp
      tests/test_lru.cpp
      tests/test_parser.cpp
      src/core/store.cpp
      src/core/lru.cpp
      src/parser/command.cpp
  )
  target_link_libraries(run_tests GTest::GTest GTest::Main pthread)
  add_test(NAME SwiftCacheTests COMMAND run_tests)
  ```
- [ ] Git init, create .gitignore (exclude build/, *.o, *.a)
- [ ] Test build works: `mkdir build && cd build && cmake .. && make`
- [ ] Push initial commit to GitHub
- [ ] Verify: `./swiftcache` compiles without errors (even if empty main)

---

### Day 2 — Thread-Safe Hash Map (Core Store)
- [ ] Write `src/core/store.h`:
  ```cpp
  #pragma once
  #include <string>
  #include <unordered_map>
  #include <mutex>
  #include <optional>

  class Store {
  public:
      void set(const std::string& key, const std::string& value);
      std::optional<std::string> get(const std::string& key);
      bool del(const std::string& key);
      bool exists(const std::string& key);
      size_t size() const;

  private:
      std::unordered_map<std::string, std::string> data_;
      mutable std::mutex mutex_;
  };
  ```
- [ ] Write `src/core/store.cpp` — implement all methods with mutex lock_guard
- [ ] **Key rule:** always lock before reading OR writing. Use `std::lock_guard<std::mutex>`
- [ ] Write `tests/test_store.cpp` using Google Test:
  ```cpp
  TEST(StoreTest, SetAndGet) {
      Store store;
      store.set("name", "aman");
      EXPECT_EQ(store.get("name"), "aman");
  }
  TEST(StoreTest, GetMissingKey) {
      Store store;
      EXPECT_EQ(store.get("missing"), std::nullopt);
  }
  TEST(StoreTest, DeleteKey) {
      Store store;
      store.set("key", "val");
      EXPECT_TRUE(store.del("key"));
      EXPECT_FALSE(store.exists("key"));
  }
  ```
- [ ] Run tests: `cd build && make && ./run_tests`
- [ ] All tests must pass before moving on
- [ ] Commit: "feat: thread-safe hash map core store"

---

### Day 3 — LRU Eviction Policy
**Concept:** LRU = Least Recently Used. When store is full, evict the
key that hasn't been accessed for the longest time.
**Data structure:** Doubly linked list + hash map = O(1) get and set.

- [ ] Write `src/core/lru.h`:
  ```cpp
  #pragma once
  #include <list>
  #include <unordered_map>
  #include <string>
  #include <mutex>
  #include <optional>

  class LRUCache {
  public:
      explicit LRUCache(size_t capacity);
      void put(const std::string& key, const std::string& value);
      std::optional<std::string> get(const std::string& key);
      bool del(const std::string& key);
      size_t size() const;
      size_t capacity() const;

  private:
      size_t capacity_;
      // list stores {key, value} pairs — front = most recent
      std::list<std::pair<std::string, std::string>> cache_list_;
      // map stores key → iterator into list (for O(1) access)
      std::unordered_map<std::string,
          std::list<std::pair<std::string,std::string>>::iterator> cache_map_;
      mutable std::mutex mutex_;

      void evict_lru();  // remove least recently used (back of list)
  };
  ```
- [ ] Implement in `src/core/lru.cpp`:
  - `put()`: if key exists → update + move to front. If full → evict back, then insert front
  - `get()`: if found → move to front (marks as recently used), return value
  - `del()`: erase from both list and map
- [ ] Write tests:
  ```cpp
  TEST(LRUTest, EvictsLeastRecentlyUsed) {
      LRUCache cache(2);
      cache.put("a", "1");
      cache.put("b", "2");
      cache.get("a");        // access "a" → "b" is now LRU
      cache.put("c", "3");   // should evict "b"
      EXPECT_EQ(cache.get("b"), std::nullopt);
      EXPECT_EQ(cache.get("a"), "1");
      EXPECT_EQ(cache.get("c"), "3");
  }
  TEST(LRUTest, O1Operations) {
      // Insert 1M items and verify it's fast — not a correctness test
      LRUCache cache(1000000);
      for (int i = 0; i < 1000000; i++)
          cache.put(std::to_string(i), std::to_string(i));
      EXPECT_EQ(cache.size(), 1000000);
  }
  ```
- [ ] All tests pass
- [ ] Commit: "feat: LRU eviction via doubly linked list + hash map O(1)"

---

### Day 4 — Command Parser
**Supported commands:**
- `SET key value` → store key-value
- `GET key` → retrieve value
- `DEL key` → delete key
- `TTL key seconds` → set expiry
- `EXISTS key` → check if key exists
- `PING` → returns PONG (health check)

- [ ] Write `src/parser/command.h`:
  ```cpp
  #pragma once
  #include <string>
  #include <vector>

  enum class CommandType {
      SET, GET, DEL, TTL, EXISTS, PING, UNKNOWN
  };

  struct Command {
      CommandType type;
      std::vector<std::string> args;  // args[0]=key, args[1]=value etc.
      bool valid;
      std::string error_msg;
  };

  class CommandParser {
  public:
      static Command parse(const std::string& raw_input);
  private:
      static std::vector<std::string> tokenize(const std::string& input);
  };
  ```
- [ ] Implement parser — tokenize by spaces, validate arg count per command
- [ ] Handle edge cases: empty input, too many args, unknown commands
- [ ] Write tests:
  ```cpp
  TEST(ParserTest, ParseSet) {
      auto cmd = CommandParser::parse("SET mykey myvalue");
      EXPECT_EQ(cmd.type, CommandType::SET);
      EXPECT_EQ(cmd.args[0], "mykey");
      EXPECT_EQ(cmd.args[1], "myvalue");
      EXPECT_TRUE(cmd.valid);
  }
  TEST(ParserTest, ParseInvalidCommand) {
      auto cmd = CommandParser::parse("SET");  // missing value
      EXPECT_FALSE(cmd.valid);
  }
  TEST(ParserTest, ParsePing) {
      auto cmd = CommandParser::parse("PING");
      EXPECT_EQ(cmd.type, CommandType::PING);
      EXPECT_TRUE(cmd.valid);
  }
  ```
- [ ] All tests pass
- [ ] Commit: "feat: lock-free command parser with full validation"

---

### Day 5 — TTL (Time-To-Live) Expiry
- [ ] Write `src/core/ttl.h`:
  ```cpp
  #pragma once
  #include <string>
  #include <unordered_map>
  #include <chrono>
  #include <mutex>

  class TTLManager {
  public:
      void set_expiry(const std::string& key, int seconds);
      bool is_expired(const std::string& key) const;
      void remove(const std::string& key);
      void cleanup_expired(LRUCache& cache);  // call periodically

  private:
      // key → expiry time point
      std::unordered_map<std::string,
          std::chrono::steady_clock::time_point> expiry_map_;
      mutable std::mutex mutex_;
  };
  ```
- [ ] Implement `is_expired()`: check if current time > expiry time point
- [ ] Implement `cleanup_expired()`: scan all keys, call cache.del() on expired ones
- [ ] In Store/LRUCache `get()`: check TTLManager before returning value. If expired → delete + return nullopt
- [ ] Run background cleanup thread every 1 second:
  ```cpp
  std::thread cleanup_thread([&]() {
      while (running_) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          ttl_manager_.cleanup_expired(cache_);
      }
  });
  cleanup_thread.detach();
  ```
- [ ] Write tests for TTL expiry
- [ ] Commit: "feat: TTL expiry with background cleanup thread"

---

### Day 6 — TCP Server (Single-Threaded First)
- [ ] Write `src/server/server.h`:
  ```cpp
  #pragma once
  #include <string>
  #include "../core/lru.h"
  #include "../core/ttl.h"
  #include "../parser/command.h"
  #include "../persistence/aof.h"

  class Server {
  public:
      Server(int port, size_t cache_capacity);
      void start();
      void stop();

  private:
      int port_;
      int server_fd_;
      bool running_;
      LRUCache cache_;
      TTLManager ttl_;
      AOFWriter aof_;

      void handle_client(int client_fd);
      std::string execute_command(const Command& cmd);
      void setup_socket();
  };
  ```
- [ ] Implement TCP server using POSIX sockets:
  ```cpp
  // In server.cpp — setup_socket()
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  // setsockopt SO_REUSEADDR
  // bind to port
  // listen(server_fd_, SOMAXCONN)
  ```
- [ ] `handle_client()`: read raw bytes → parse command → execute → send response
- [ ] `execute_command()`: switch on CommandType → call cache methods → return response string
- [ ] Response format (simple, like Redis):
  - OK responses: `+OK\r\n`
  - String values: `$<len>\r\n<value>\r\n`
  - Not found: `$-1\r\n`
  - Errors: `-ERR <message>\r\n`
- [ ] Test manually: `nc localhost 6380` then type `SET key value` → should get `+OK`
- [ ] Commit: "feat: single-threaded TCP server with POSIX sockets"

---

### Day 7 — Multi-Threaded Server
**This is the key upgrade** — one thread per client connection.

- [ ] Update `server.cpp` to spawn a thread per client:
  ```cpp
  while (running_) {
      int client_fd = accept(server_fd_, nullptr, nullptr);
      if (client_fd < 0) continue;

      // spawn thread for each client
      std::thread client_thread([this, client_fd]() {
          handle_client(client_fd);
          close(client_fd);
      });
      client_thread.detach();
  }
  ```
- [ ] The LRUCache already has mutex — safe for concurrent access
- [ ] Add connection counter with `std::atomic<int>`:
  ```cpp
  std::atomic<int> active_connections_{0};
  // increment on accept, decrement when client disconnects
  ```
- [ ] Add thread pool alternative (optional but impressive):
  - Fixed pool of N worker threads
  - Shared work queue with condition variable
  - Workers pick up client connections from queue
- [ ] Test: open 10 simultaneous `nc` connections → all work correctly
- [ ] Commit: "feat: multi-threaded TCP server with thread-per-client"

---

## Week 2 — AOF Persistence + gRPC Interface (Days 8–14)

### Day 8–9 — AOF (Append-Only File) Persistence
**Concept:** Every write command (SET, DEL, TTL) is appended to a log
file. On startup, replay the log to restore state. Same as Redis AOF mode.

- [ ] Write `src/persistence/aof.h`:
  ```cpp
  #pragma once
  #include <fstream>
  #include <string>
  #include <mutex>
  #include <vector>

  class AOFWriter {
  public:
      explicit AOFWriter(const std::string& filepath);
      void log_command(const std::string& raw_command);
      void fsync_now();  // force flush to disk
      ~AOFWriter();

  private:
      std::ofstream file_;
      std::mutex mutex_;
      std::string filepath_;
  };

  class AOFReader {
  public:
      explicit AOFReader(const std::string& filepath);
      std::vector<std::string> read_all_commands();
  };
  ```
- [ ] `log_command()`: write command string + newline to file, call file_.flush()
- [ ] In Server `execute_command()`: after every SET/DEL/TTL → call `aof_.log_command(raw)`
- [ ] On Server startup: read AOF file → replay all commands → cache restored
  ```cpp
  // In Server constructor
  AOFReader reader("swiftcache.aof");
  for (const auto& cmd_str : reader.read_all_commands()) {
      auto cmd = CommandParser::parse(cmd_str);
      execute_command(cmd);  // replay without re-logging
  }
  ```
- [ ] Write tests:
  - Set 5 keys → stop server → start fresh server → all 5 keys still present
- [ ] Commit: "feat: AOF disk persistence with crash recovery replay"

---

### Day 10–13 — gRPC Interface
**Concept:** Expose same KV operations via gRPC alongside TCP.
Clients can choose TCP (raw, fastest) or gRPC (typed, safer).

- [ ] Write `proto/cache.proto`:
  ```protobuf
  syntax = "proto3";
  package swiftcache;

  service CacheService {
      rpc Set(SetRequest) returns (SetResponse);
      rpc Get(GetRequest) returns (GetResponse);
      rpc Del(DelRequest) returns (DelResponse);
      rpc Ping(PingRequest) returns (PingResponse);
  }

  message SetRequest {
      string key = 1;
      string value = 2;
      int32 ttl_seconds = 3;  // 0 = no expiry
  }
  message SetResponse { bool success = 1; }

  message GetRequest { string key = 1; }
  message GetResponse {
      bool found = 1;
      string value = 2;
  }

  message DelRequest { string key = 1; }
  message DelResponse { bool deleted = 1; }

  message PingRequest {}
  message PingResponse { string message = 1; }
  ```

- [ ] Generate C++ code from proto:
  ```bash
  protoc --grpc_out=src/grpc/ --cpp_out=src/grpc/ \
      --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` proto/cache.proto
  ```

- [ ] Write `src/grpc/cache_service.h`:
  ```cpp
  #pragma once
  #include <grpcpp/grpcpp.h>
  #include "cache.grpc.pb.h"
  #include "../core/lru.h"
  #include "../core/ttl.h"

  class CacheServiceImpl final : public swiftcache::CacheService::Service {
  public:
      CacheServiceImpl(LRUCache& cache, TTLManager& ttl)
          : cache_(cache), ttl_(ttl) {}

      grpc::Status Set(grpc::ServerContext* ctx,
          const swiftcache::SetRequest* req,
          swiftcache::SetResponse* res) override;

      grpc::Status Get(grpc::ServerContext* ctx,
          const swiftcache::GetRequest* req,
          swiftcache::GetResponse* res) override;

      grpc::Status Del(grpc::ServerContext* ctx,
          const swiftcache::DelRequest* req,
          swiftcache::DelResponse* res) override;

      grpc::Status Ping(grpc::ServerContext* ctx,
          const swiftcache::PingRequest* req,
          swiftcache::PingResponse* res) override;

  private:
      LRUCache& cache_;
      TTLManager& ttl_;
  };
  ```

- [ ] Implement each RPC method — delegate to same LRUCache as TCP server
- [ ] Start gRPC server on a different port (e.g. 6381) alongside TCP:
  ```cpp
  // In main.cpp — run both servers concurrently
  std::thread grpc_thread([&]() {
      grpc::ServerBuilder builder;
      builder.AddListeningPort("0.0.0.0:6381", grpc::InsecureServerCredentials());
      builder.RegisterService(&grpc_service);
      auto grpc_server = builder.BuildAndStart();
      grpc_server->Wait();
  });
  ```
- [ ] Update CMakeLists.txt to link grpc++ and protobuf
- [ ] Write Python gRPC client `client/grpc_client.py`:
  ```python
  import grpc
  import cache_pb2
  import cache_pb2_grpc

  channel = grpc.insecure_channel('localhost:6381')
  stub = cache_pb2_grpc.CacheServiceStub(channel)

  # Set
  resp = stub.Set(cache_pb2.SetRequest(key='name', value='aman', ttl_seconds=60))
  print('Set:', resp.success)

  # Get
  resp = stub.Get(cache_pb2.GetRequest(key='name'))
  print('Get:', resp.value if resp.found else 'NOT FOUND')

  # Ping
  resp = stub.Ping(cache_pb2.PingRequest())
  print('Ping:', resp.message)
  ```
- [ ] Test: run server → run grpc_client.py → see correct responses
- [ ] Commit: "feat: gRPC interface alongside TCP server using Protocol Buffers"

---

### Day 14 — Python TCP Client Library
- [ ] Write `client/client.py` — clean Python library:
  ```python
  import socket

  class SwiftCacheClient:
      def __init__(self, host='localhost', port=6380):
          self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
          self.sock.connect((host, port))

      def set(self, key: str, value: str) -> bool:
          self._send(f'SET {key} {value}')
          return self._recv() == '+OK'

      def get(self, key: str) -> str | None:
          self._send(f'GET {key}')
          resp = self._recv()
          return None if resp == '$-1' else resp

      def delete(self, key: str) -> bool:
          self._send(f'DEL {key}')
          return self._recv() == '+OK'

      def ping(self) -> bool:
          self._send('PING')
          return self._recv() == '+PONG'

      def ttl(self, key: str, seconds: int) -> bool:
          self._send(f'TTL {key} {seconds}')
          return self._recv() == '+OK'

      def _send(self, cmd: str):
          self.sock.sendall((cmd + '\r\n').encode())

      def _recv(self) -> str:
          return self.sock.recv(1024).decode().strip()

      def close(self):
          self.sock.close()
  ```
- [ ] Write usage example in README
- [ ] Commit: "feat: Python TCP client library"

---

## Week 3 — Benchmarks + Tests + CI/CD + README (Days 15–21)

### Day 15–16 — Google Benchmark
**This is what gives you real resume numbers.**

- [ ] Write `benchmarks/bench_store.cpp`:
  ```cpp
  #include <benchmark/benchmark.h>
  #include "../src/core/lru.h"

  // Single-threaded SET benchmark
  static void BM_Set(benchmark::State& state) {
      LRUCache cache(1000000);
      int i = 0;
      for (auto _ : state) {
          cache.put(std::to_string(i++), "value");
      }
      state.SetItemsProcessed(state.iterations());
  }
  BENCHMARK(BM_Set);

  // Single-threaded GET benchmark
  static void BM_Get(benchmark::State& state) {
      LRUCache cache(1000000);
      for (int i = 0; i < 1000000; i++)
          cache.put(std::to_string(i), "value");
      int i = 0;
      for (auto _ : state) {
          benchmark::DoNotOptimize(cache.get(std::to_string(i++ % 1000000)));
      }
      state.SetItemsProcessed(state.iterations());
  }
  BENCHMARK(BM_Get);

  // Multi-threaded SET benchmark
  static void BM_Set_MultiThread(benchmark::State& state) {
      static LRUCache cache(1000000);
      int i = 0;
      for (auto _ : state) {
          cache.put(std::to_string(i++), "value");
      }
      state.SetItemsProcessed(state.iterations());
  }
  BENCHMARK(BM_Set_MultiThread)->Threads(4)->Threads(8);

  BENCHMARK_MAIN();
  ```
- [ ] Add to CMakeLists.txt:
  ```cmake
  find_package(benchmark REQUIRED)
  add_executable(bench benchmarks/bench_store.cpp src/core/lru.cpp)
  target_link_libraries(bench benchmark::benchmark pthread)
  ```
- [ ] Run: `./bench --benchmark_format=json > benchmark_results.json`
- [ ] Run: `./bench --benchmark_out=results.json --benchmark_out_format=json`
- [ ] **Screenshot the output** — this is your real resume metric
- [ ] Compare single-threaded vs multi-threaded throughput (ops/sec)
- [ ] Record: "X million ops/sec single-threaded, Y million ops/sec with 4 threads"
- [ ] Commit: "bench: Google Benchmark results — X ops/sec throughput"

---

### Day 17 — Comprehensive Tests
- [ ] Write tests for every module — target 20+ passing tests:
  ```
  StoreTest:
    test_set_and_get
    test_get_missing_key
    test_delete_existing_key
    test_delete_missing_key
    test_exists
    test_concurrent_set_get (10 threads simultaneously)

  LRUTest:
    test_evicts_lru_on_capacity
    test_get_updates_recency
    test_put_updates_existing_key
    test_capacity_one
    test_o1_operations_1M_items

  ParserTest:
    test_parse_set_valid
    test_parse_get_valid
    test_parse_del_valid
    test_parse_ttl_valid
    test_parse_ping
    test_parse_set_missing_value
    test_parse_unknown_command
    test_parse_empty_input

  TTLTest:
    test_key_expires_after_ttl
    test_key_accessible_before_expiry
    test_cleanup_removes_expired

  AOFTest:
    test_commands_written_to_file
    test_replay_restores_state
    test_server_recovers_after_restart
  ```
- [ ] Concurrency test (important for systems role interviews):
  ```cpp
  TEST(StoreTest, ConcurrentSetGet) {
      LRUCache cache(100000);
      std::vector<std::thread> threads;

      // 10 writer threads
      for (int t = 0; t < 10; t++) {
          threads.emplace_back([&cache, t]() {
              for (int i = 0; i < 10000; i++)
                  cache.put(std::to_string(t * 10000 + i), "val");
          });
      }
      for (auto& th : threads) th.join();
      EXPECT_EQ(cache.size(), 100000);
  }
  ```
- [ ] Run: `./run_tests --gtest_verbose`
- [ ] Screenshot all green
- [ ] Commit: "test: 20+ unit tests including concurrency tests"

---

### Day 18 — GitHub Actions CI/CD
- [ ] Write `.github/workflows/ci.yml`:
  ```yaml
  name: CI

  on: [push, pull_request]

  jobs:
    build-and-test:
      runs-on: ubuntu-latest

      steps:
        - uses: actions/checkout@v3

        - name: Install dependencies
          run: |
            sudo apt update
            sudo apt install -y build-essential cmake libgtest-dev \
              libbenchmark-dev libgrpc++-dev libprotobuf-dev \
              protobuf-compiler-grpc

        - name: Configure CMake
          run: cmake -B build -DCMAKE_BUILD_TYPE=Release

        - name: Build
          run: cmake --build build --parallel

        - name: Run tests
          run: cd build && ctest --verbose

        - name: Run benchmarks (quick smoke test)
          run: ./build/bench --benchmark_min_time=0.1
  ```
- [ ] Push → verify green checkmark on GitHub Actions tab
- [ ] Screenshot passing CI — add to README
- [ ] Commit: "ci: GitHub Actions build + test pipeline"

---

### Day 19–20 — Performance Tuning + Real Metrics
- [ ] **Measure single-threaded baseline:**
  ```bash
  ./bench --benchmark_filter=BM_Set$ 2>&1 | tee single_thread.txt
  ```
- [ ] **Measure multi-threaded:**
  ```bash
  ./bench --benchmark_filter=BM_Set_MultiThread 2>&1 | tee multi_thread.txt
  ```
- [ ] Calculate: multi-thread ops/sec ÷ single-thread ops/sec = your multiplier
- [ ] If < 2x: check for lock contention — consider reader-writer locks (`std::shared_mutex`) for GET operations:
  ```cpp
  // reads can be concurrent — use shared lock
  std::shared_lock<std::shared_mutex> lock(mutex_);  // for get()
  // writes are exclusive
  std::unique_lock<std::shared_mutex> lock(mutex_);  // for put()/del()
  ```
- [ ] Re-run after shared_mutex — throughput should improve
- [ ] **Measure TCP throughput** using custom benchmark:
  ```python
  # benchmarks/tcp_bench.py
  import socket, time

  client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  client.connect(('localhost', 6380))

  N = 100000
  start = time.time()
  for i in range(N):
      client.sendall(f'SET key{i} value{i}\r\n'.encode())
      client.recv(1024)
  elapsed = time.time() - start
  print(f'{N/elapsed:.0f} ops/sec over TCP')
  ```
- [ ] Record: "X ops/sec over TCP, Y million ops/sec in-process"
- [ ] **Save all benchmark outputs** — these are your real resume numbers
- [ ] Update resume bullet with real figures

---

### Day 21 — README + GitHub Polish
- [ ] Write `README.md`:
  ```markdown
  # SwiftCache

  A Redis-like in-memory key-value store built from scratch in C++17.
  Supports TCP and gRPC interfaces, LRU eviction, TTL expiry,
  AOF persistence, and concurrent client connections.

  ## Features
  - GET / SET / DEL / TTL / EXISTS / PING commands
  - LRU eviction (doubly linked list + hash map, O(1))
  - TTL-based key expiry with background cleanup
  - AOF persistence — crash recovery on restart
  - Multi-threaded TCP server (one thread per client)
  - gRPC interface alongside TCP
  - Python client library (TCP + gRPC)

  ## Architecture
  [insert diagram — draw with excalidraw.com]

  ## Benchmarks
  | Operation | Single-thread | 4 threads | 8 threads |
  |-----------|--------------|-----------|-----------|
  | SET       | X M ops/sec  | Y M ops/sec | Z M ops/sec |
  | GET       | X M ops/sec  | Y M ops/sec | Z M ops/sec |

  [insert screenshot of Google Benchmark output]

  ## Build & Run
  \`\`\`bash
  mkdir build && cd build && cmake .. && make
  ./swiftcache --port 6380 --grpc-port 6381 --capacity 1000000
  \`\`\`

  ## Usage — TCP
  \`\`\`bash
  nc localhost 6380
  SET name aman
  GET name
  TTL name 60
  DEL name
  PING
  \`\`\`

  ## Usage — Python Client
  \`\`\`python
  from client.client import SwiftCacheClient
  c = SwiftCacheClient()
  c.set('name', 'aman')
  print(c.get('name'))  # aman
  \`\`\`

  ## Running Tests
  \`\`\`bash
  cd build && ./run_tests
  \`\`\`
  [insert screenshot of all tests passing]

  ## CI Status
  [GitHub Actions badge]
  ```
- [ ] Add GitHub Actions badge to README top:
  ```markdown
  ![CI](https://github.com/yourusername/SwiftCache/actions/workflows/ci.yml/badge.svg)
  ```
- [ ] Add architecture diagram (draw.io or excalidraw — free)
- [ ] Pin repo on GitHub profile
- [ ] Add repo description: "Redis-like KV store in C++ — TCP + gRPC, LRU eviction, AOF persistence"
- [ ] Commit: "docs: complete README with benchmarks and architecture"

---

## Resume Bullet Template (fill with real numbers after Day 19)

```
- Engineered a Redis-like in-memory KV store in C++ with a
  multi-threaded TCP server (GET/SET/DEL/TTL); implemented LRU eviction
  via doubly linked list + hash map (O(1)) and AOF disk persistence
  for crash recovery — <X> million ops/sec single-threaded.

- Exposed a gRPC interface alongside TCP for typed, high-performance
  client communication; replaced std::mutex with std::shared_mutex for
  read-heavy workloads achieving <Y>x throughput improvement over
  single-threaded baseline across <N> concurrent clients.
```

---

## Interview Questions You Can Now Answer

After building this, you can confidently answer:
- "How does Redis handle LRU eviction?" — you implemented it
- "What is AOF persistence and how does it work?" — you built it
- "How do you handle concurrent access in C++?" — mutex vs shared_mutex
- "What's the difference between TCP and gRPC?" — you used both
- "How would you benchmark a systems component?" — Google Benchmark
- "What happens to a cache when the server crashes?" — AOF replay

---

## If You Get Stuck — What to Tell the Next AI

```
Project: SwiftCache — Redis-like KV store in C++17
Stack: C++17, CMake, POSIX sockets, gRPC + Protobuf,
       std::mutex/shared_mutex, Google Test, Google Benchmark
Components built so far: [LIST WHAT YOU FINISHED]
Current day: [DAY NUMBER]
Current task: [PASTE THE TASK]
Error: [PASTE EXACT ERROR MESSAGE AND FILE/LINE]
```

---

## Daily Commit Checklist
- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] All existing tests still pass before committing
- [ ] No hardcoded paths or machine-specific settings
- [ ] Commit message: feat/fix/test/bench/docs/chore
- [ ] Push to GitHub at end of every day

---

## Cost — Everything Is Free
- C++, CMake, gRPC, Protobuf — free, open source
- Google Test, Google Benchmark — free, open source
- GitHub + GitHub Actions — free for public repos
- No cloud deployment needed — runs locally, demo via terminal

---

*Built to demonstrate: systems programming, concurrency, TCP networking,
gRPC/Protobuf, LRU data structures, disk persistence, performance
benchmarking, and production-grade C++ engineering.*
