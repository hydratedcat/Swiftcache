#!/usr/bin/env python3
"""SwiftCache TCP client — pure stdlib, zero dependencies.

Speaks the Redis-style text protocol over a persistent TCP connection.
All commands are synchronous: send command → read full response → return.

Response protocol handled:
    +OK\r\n            — simple string (success)
    +PONG\r\n          — simple string (ping reply)
    $<len>\r\n<val>\r\n — bulk string (value payload)
    $-1\r\n            — null bulk string (key not found)
    :<int>\r\n         — integer (EXISTS returns 0 or 1)
    -ERR msg\r\n       — error (raises SwiftCacheError)
"""

import socket
from typing import Optional, Union


class SwiftCacheError(Exception):
    """Raised when the server returns an error response (-ERR ...)."""


class SwiftCacheClient:
    """TCP client for SwiftCache.

    Usage:
        client = SwiftCacheClient()
        client.set("name", "aman")
        print(client.get("name"))  # "aman"
        client.close()

    Or as a context manager:
        with SwiftCacheClient() as c:
            c.set("name", "aman")
            print(c.get("name"))
    """

    def __init__(
        self, host: str = "localhost", port: int = 6380, timeout: float = 5.0
    ) -> None:
        self._host = host
        self._port = port
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        self._sock.connect((host, port))
        self._buffer = ""

    # -- Context manager --------------------------------------------------

    def __enter__(self) -> "SwiftCacheClient":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.close()
        return False

    # -- Public API -------------------------------------------------------

    def set(self, key: str, value: str) -> bool:
        """SET key value → True on success."""
        self._send(f"SET {key} {value}")
        return self._read_response() == "OK"

    def get(self, key: str) -> Optional[str]:
        """GET key → value string, or None if not found / expired."""
        self._send(f"GET {key}")
        return self._read_response()

    def delete(self, key: str) -> bool:
        """DEL key → True if key existed and was deleted."""
        self._send(f"DEL {key}")
        resp = self._read_response()
        return resp == "OK"

    def exists(self, key: str) -> bool:
        """EXISTS key → True if key exists in cache."""
        self._send(f"EXISTS {key}")
        return self._read_response() == 1

    def ttl(self, key: str, seconds: int) -> bool:
        """TTL key seconds → True on success."""
        self._send(f"TTL {key} {seconds}")
        return self._read_response() == "OK"

    def ping(self) -> bool:
        """PING → True if server responds with PONG."""
        self._send("PING")
        return self._read_response() == "PONG"

    def close(self) -> None:
        """Close the TCP connection."""
        try:
            self._sock.close()
        except OSError:
            pass

    # -- Internal protocol handling ---------------------------------------

    def _send(self, cmd: str) -> None:
        """Send a command line terminated by \\r\\n."""
        self._sock.sendall((cmd + "\r\n").encode())

    def _read_line(self) -> str:
        """Read one \\r\\n-terminated line from the socket.

        Uses an internal buffer to handle partial reads — recv() may
        return less than a full line, or multiple lines at once.
        """
        while "\r\n" not in self._buffer:
            chunk = self._sock.recv(4096).decode()
            if not chunk:
                raise ConnectionError("Server closed connection")
            self._buffer += chunk

        line, self._buffer = self._buffer.split("\r\n", 1)
        return line

    def _read_response(self) -> Union[str, int, None]:
        """Parse one Redis-style response from the wire.

        Returns:
            str  — simple strings (+OK → "OK", +PONG → "PONG")
            str  — bulk strings  ($4\\r\\naman → "aman")
            None — null bulk string ($-1 → key not found)
            int  — integers (:1 → 1, :0 → 0)

        Raises:
            SwiftCacheError — server error responses (-ERR ...)
            ConnectionError — connection dropped mid-read
        """
        line = self._read_line()

        if not line:
            raise ConnectionError("Empty response from server")

        prefix = line[0]
        payload = line[1:]

        if prefix == "+":
            # Simple string: +OK, +PONG
            return payload

        if prefix == "-":
            # Error: -ERR message
            raise SwiftCacheError(payload)

        if prefix == ":":
            # Integer: :0, :1
            return int(payload)

        if prefix == "$":
            # Bulk string: $<len>\r\n<data>\r\n  or  $-1\r\n (null)
            length = int(payload)
            if length < 0:
                return None
            # Read exactly `length` chars of payload
            data_line = self._read_line()
            return data_line[:length]

        raise SwiftCacheError(f"Unknown response prefix: {prefix!r}")

    # -- repr for debugging -----------------------------------------------

    def __repr__(self) -> str:
        return f"SwiftCacheClient(host={self._host!r}, port={self._port})"


# -------------------------------------------------------------------------
# Demo — requires a running SwiftCache server on localhost:6380
# -------------------------------------------------------------------------
if __name__ == "__main__":
    print("SwiftCache TCP Client — Demo\n")

    try:
        with SwiftCacheClient() as c:
            # Health check
            assert c.ping(), "Ping failed"
            print("✓ PING  → PONG")

            # SET + GET
            assert c.set("name", "aman"), "Set failed"
            print("✓ SET   name = aman")

            val = c.get("name")
            assert val == "aman", f"Expected 'aman', got {val!r}"
            print(f"✓ GET   name → {val!r}")

            # EXISTS
            assert c.exists("name"), "Exists should be True"
            print("✓ EXISTS name → True")

            # SET second key + DEL
            c.set("city", "delhi")
            print("✓ SET   city = delhi")

            assert c.delete("city"), "Del should return True"
            print("✓ DEL   city → True")

            val = c.get("city")
            assert val is None, f"Expected None, got {val!r}"
            print("✓ GET   city → None (deleted)")

            assert not c.exists("city"), "Exists should be False"
            print("✓ EXISTS city → False")

            # DEL non-existent key
            assert not c.delete("nonexistent"), "Del missing key should return False"
            print("✓ DEL   nonexistent → False")

            # TTL
            c.set("temp", "ephemeral")
            assert c.ttl("temp", 60), "TTL set failed"
            print("✓ TTL   temp 60 → OK")

            # Error handling
            try:
                c._send("FOOBAR")
                c._read_response()
                print("✗ Expected error for unknown command")
            except SwiftCacheError as e:
                print(f"✓ ERROR FOOBAR → {e}")

            print("\n✅ All TCP operations completed successfully.")

    except ConnectionRefusedError:
        print("✗ Could not connect — is SwiftCache running on localhost:6380?")
        print("  Start with: cd build && ./swiftcache")
