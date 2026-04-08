# SwiftCache — Study Guide

## Day 1 — Project Setup + CMake

### Decisions & Rationale

**Build System: CMake**
- Why: Industry-standard for C++ projects. Generates Makefiles, handles dependency discovery (`find_package`), and supports cross-platform builds.
- Alternative considered: Makefile (too manual for multi-target projects with dependencies like gRPC, GTest).

**Folder Structure: Feature-based**
- `src/core/` — data structures (Store, LRU, TTL)
- `src/server/` — networking (TCP server, connection handling)
- `src/parser/` — protocol parsing (command tokenization)
- `src/persistence/` — disk I/O (AOF)
- `src/grpc/` — RPC layer
- Why: High cohesion per module. Each folder can be reasoned about independently.

### Terminal Commands Explained

| Command | What it does |
|---------|-------------|
| `cmake ..` | Reads `CMakeLists.txt` from parent dir, generates Makefiles in current dir (`build/`) |
| `cmake --build .` | Compiles all targets defined in CMakeLists.txt using the generated build system |
| `cmake -DCMAKE_BUILD_TYPE=Release` | Sets optimization level to `-O2`/`-O3` (vs Debug which uses `-g -O0`) |
| `ctest` | Runs all tests registered with `add_test()` in CMake |
| `find_package(GTest REQUIRED)` | Locates Google Test headers/libs installed on the system; fails if not found |

### Big O — Day 1
No algorithms yet. Project scaffold only.
