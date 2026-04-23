#!/usr/bin/env python3
"""SwiftCache gRPC client — demonstrates all CacheService RPCs."""

import sys
import os

# Add proto-generated stubs to path
sys.path.insert(0, os.path.dirname(__file__))

import grpc

# Generated from proto/cache.proto via:
#   python3 -m grpc_tools.protoc -I proto --python_out=client/ --grpc_python_out=client/ proto/cache.proto
import cache_pb2
import cache_pb2_grpc


def main():
    channel = grpc.insecure_channel("localhost:6381")
    stub = cache_pb2_grpc.CacheServiceStub(channel)

    # Ping
    resp = stub.Ping(cache_pb2.PingRequest())
    print(f"Ping: {resp.message}")

    # Set (no TTL)
    resp = stub.Set(cache_pb2.SetRequest(key="name", value="aman"))
    print(f"Set name=aman: success={resp.success}")

    # Set with TTL
    resp = stub.Set(
        cache_pb2.SetRequest(key="temp", value="ephemeral", ttl_seconds=60)
    )
    print(f"Set temp=ephemeral (ttl=60s): success={resp.success}")

    # Get existing key
    resp = stub.Get(cache_pb2.GetRequest(key="name"))
    print(f"Get name: found={resp.found}, value={resp.value!r}")

    # Get missing key
    resp = stub.Get(cache_pb2.GetRequest(key="nonexistent"))
    print(f"Get nonexistent: found={resp.found}")

    # Del
    resp = stub.Del(cache_pb2.DelRequest(key="name"))
    print(f"Del name: deleted={resp.deleted}")

    # Verify deletion
    resp = stub.Get(cache_pb2.GetRequest(key="name"))
    print(f"Get name (after del): found={resp.found}")

    channel.close()
    print("\nAll gRPC operations completed successfully.")


if __name__ == "__main__":
    main()
