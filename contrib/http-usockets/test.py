"""Exercise the prototype through real TCP connections on loopback."""

import selectors
import socket
import subprocess
import sys

OK = b"HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"
BAD = b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
TOO_LARGE = (
    b"HTTP/1.1 431 Request Header Fields Too Large\r\n"
    b"Content-Length: 0\r\nConnection: close\r\n\r\n"
)
REQUEST = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"


def check(actual, expected):
    if actual != expected:
        raise AssertionError(f"expected {expected!r}, got {actual!r}")


def read_response(client):
    with client.makefile("rb") as stream:
        return stream.read(512)


def run(port):
    def connect():
        return socket.create_connection(("127.0.0.1", port), timeout=3)

    with connect() as partial, connect() as independent:
        partial.sendall(b"GET / HTTP/1.1\r\nHo")
        independent.sendall(REQUEST)
        check(read_response(independent), OK)

        partial.sendall(b"st: localhost\r\n\r")
        with selectors.DefaultSelector() as ready:
            ready.register(partial, selectors.EVENT_READ)
            if ready.select(0.15):
                raise AssertionError("server replied before the final LF")
        partial.sendall(b"\n")
        check(read_response(partial), OK)

    padded = b"GET / HTTP/1.1\r\nHost: localhost\r\nX-Pad: "
    exact = padded + b"a" * (8192 - len(padded) - 4) + b"\r\n\r\n"
    cases = (
        (REQUEST + REQUEST, OK),
        (REQUEST[:-2] + b"cOnTeNt-LeNgTh: 0\r\n\r\n", OK),
        (REQUEST[:-2] + b"cOnTeNt-LeNgTh: 4\r\n\r\ndata", BAD),
        (REQUEST[:-2] + b"TRANSFER-ENCODING: chunked\r\n\r\n", BAD),
        (REQUEST[:-2] + b"Content-Length: 0\r\ncontent-length: 0\r\n\r\n", BAD),
        (REQUEST[:-2] + b"host: example.com\r\n\r\n", BAD),
        (b"GET / HTTP/1.1\r\n\r\n", BAD),
        (b"GET / HTTP/1.1\r\nHost : localhost\r\n\r\n", BAD),
        (exact, OK),
        (exact + REQUEST, OK),
        (exact[:-4] + b"a\r\n\r\n", TOO_LARGE),
    )
    for request, expected in cases:
        with connect() as client:
            client.sendall(request)
            check(read_response(client), expected)

    with connect() as client:
        client.sendall(REQUEST[:-1])
        client.shutdown(socket.SHUT_WR)
        check(read_response(client), b"")


def main():
    with subprocess.Popen([sys.argv[1]], stdout=subprocess.PIPE) as server:
        try:
            with selectors.DefaultSelector() as ready:
                ready.register(server.stdout, selectors.EVENT_READ)
                if not ready.select(5):
                    raise TimeoutError("prototype did not announce its port")
            announcement = server.stdout.readline()
            if not announcement.strip().isdigit():
                raise RuntimeError(f"prototype did not start: {announcement!r}")
            run(int(announcement))
        finally:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
    print("uSockets HTTP checks passed")


if __name__ == "__main__":
    main()
