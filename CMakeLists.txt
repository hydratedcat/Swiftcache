cmake_minimum_required(VERSION 3.15)
project(SwiftCache VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Default build type — overridable: cmake .. -DCMAKE_BUILD_TYPE=Asan
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Sanitizer build type
set(CMAKE_CXX_FLAGS_ASAN "-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags for AddressSanitizer + UBSan build" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined"
    CACHE STRING "Linker flags for Asan build" FORCE)

# Compiler flags
add_compile_options(-Wall -Wextra -pthread)

# ---------------------------------------------------------------------------
# gRPC + Protobuf
# ---------------------------------------------------------------------------
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)

# Generated proto sources (pre-generated via protoc)
set(PROTO_SRCS
    src/grpc/cache.pb.cc
    src/grpc/cache.grpc.pb.cc
)
set(PROTO_HDRS
    src/grpc/cache.pb.h
    src/grpc/cache.grpc.pb.h
)

# ---------------------------------------------------------------------------
# Main server executable
# ---------------------------------------------------------------------------
add_executable(swiftcache
    src/server/main.cpp
    src/server/server.cpp
    src/server/connection.cpp
    src/core/store.cpp
    src/core/lru.cpp
    src/core/ttl.cpp
    src/parser/command.cpp
    src/persistence/aof.cpp
    src/grpc/cache_service.cpp
    ${PROTO_SRCS}
)
target_include_directories(swiftcache PRIVATE
    ${Protobuf_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)
target_link_libraries(swiftcache
    pthread
    ${Protobuf_LIBRARIES}
    ${GRPC_LIBRARIES}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
find_package(GTest REQUIRED)

add_executable(run_tests
    tests/test_store.cpp
    tests/test_lru.cpp
    tests/test_parser.cpp
    tests/test_ttl.cpp
    tests/test_server.cpp
    tests/test_aof.cpp
    tests/test_grpc.cpp
    tests/test_comprehensive.cpp
    src/core/store.cpp
    src/core/lru.cpp
    src/core/ttl.cpp
    src/parser/command.cpp
    src/server/server.cpp
    src/server/connection.cpp
    src/persistence/aof.cpp
    src/grpc/cache_service.cpp
    ${PROTO_SRCS}
)
target_include_directories(run_tests PRIVATE
    ${Protobuf_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)
target_link_libraries(run_tests
    GTest::GTest
    GTest::Main
    pthread
    ${Protobuf_LIBRARIES}
    ${GRPC_LIBRARIES}
)
add_test(NAME SwiftCacheTests COMMAND run_tests)

# ---------------------------------------------------------------------------
# Benchmarks (Google Benchmark)
# ---------------------------------------------------------------------------
find_package(benchmark REQUIRED)

add_executable(bench
    benchmarks/bench_store.cpp
    src/core/lru.cpp
)
target_link_libraries(bench
    benchmark::benchmark
    pthread
)

# Custom target: cmake --build . --target run_bench
# Runs the benchmark and saves output to benchmarks/bench_output.txt
add_custom_target(run_bench
    COMMAND bench
        --benchmark_out=${CMAKE_SOURCE_DIR}/benchmarks/bench_output.txt
        --benchmark_out_format=console
    DEPENDS bench
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Google Benchmark suite..."
    VERBATIM
)
