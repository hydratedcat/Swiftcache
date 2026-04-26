#!/usr/bin/env python3
"""SwiftCache TCP throughput benchmark.

Measures end-to-end SET and GET operations per second over TCP.
Requires a running SwiftCache server on localhost:6380.

Usage:
    cd build && ./swiftcache &       # start server
    python3 benchmarks/tcp_bench.py  # run benchmark
"""

import socket
import time
import sys
import os

# Add client directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "client"))

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
HOST = "localhost"
PORT = 6380
SET_OPS = 100000
GET_OPS = 100000
VALUE_SIZE = 64  # bytes — realistic small value


def raw_bench(host: str, port: int, n_set: int, n_get: int) -> None:
    """Run SET then GET benchmark using raw sockets (no client overhead)."""

    value = "v" * VALUE_SIZE

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)
    sock.connect((host, port))

    # Warm up — discard any startup latency
    sock.sendall(b"PING\r\n")
    sock.recv(4096)

    # --- SET benchmark ---
    print(f"Running {n_set:,} SET operations...")
    start = time.perf_counter()
    for i in range(n_set):
        sock.sendall(f"SET key{i} {value}\r\n".encode())
        sock.recv(4096)
    set_elapsed = time.perf_counter() - start
    set_ops_sec = n_set / set_elapsed

    # --- GET benchmark (100% hit rate) ---
    print(f"Running {n_get:,} GET operations...")
    start = time.perf_counter()
    for i in range(n_get):
        sock.sendall(f"GET key{i % n_set}\r\n".encode())
        sock.recv(4096)
    get_elapsed = time.perf_counter() - start
    get_ops_sec = n_get / get_elapsed

    # --- Mixed 50/50 benchmark ---
    mixed_ops = n_set
    print(f"Running {mixed_ops:,} mixed SET+GET operations...")
    start = time.perf_counter()
    for i in range(mixed_ops):
        if i % 2 == 0:
            sock.sendall(f"SET mkey{i} {value}\r\n".encode())
        else:
            sock.sendall(f"GET key{i % n_set}\r\n".encode())
        sock.recv(4096)
    mixed_elapsed = time.perf_counter() - start
    mixed_ops_sec = mixed_ops / mixed_elapsed

    sock.close()

    # --- Results ---
    print("\n" + "=" * 60)
    print("SwiftCache TCP Benchmark Results")
    print("=" * 60)
    print(f"  Value size:    {VALUE_SIZE} bytes")
    print(f"  SET ops:       {n_set:>10,}")
    print(f"  GET ops:       {n_get:>10,}")
    print()
    print(f"  SET throughput: {set_ops_sec:>10,.0f} ops/sec  ({set_elapsed:.2f}s)")
    print(f"  GET throughput: {get_ops_sec:>10,.0f} ops/sec  ({get_elapsed:.2f}s)")
    print(f"  Mixed (50/50): {mixed_ops_sec:>10,.0f} ops/sec  ({mixed_elapsed:.2f}s)")
    print("=" * 60)
    print()
    print("Note: These numbers include TCP round-trip latency.")
    print("In-process benchmarks (./bench) measure raw cache ops without network overhead.")


if __name__ == "__main__":
    print("SwiftCache TCP Benchmark\n")
    try:
        raw_bench(HOST, PORT, SET_OPS, GET_OPS)
    except ConnectionRefusedError:
        print("✗ Could not connect — is SwiftCache running on localhost:6380?")
        print("  Start with: cd build && ./swiftcache")
        sys.exit(1)
