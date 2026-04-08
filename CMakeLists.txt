cmake_minimum_required(VERSION 3.15)
project(SwiftCache VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)

# Compiler flags
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
