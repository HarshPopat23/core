# HTTP over uSockets prototype

This Linux loopback experiment calls Core's HTTP/1.1 request-line and header
parser directly from a uSockets data callback. Each connection buffers up to
8 KiB. Incomplete input stays buffered for the next read. The server handles
one bodyless `GET /` per connection, returns a fixed response, and closes the
response stream. Header names used for the prototype's policy are compared
case-insensitively. A single, nonempty Host is required, Transfer-Encoding is
rejected, and Content-Length must be absent or a single literal `0`.

Bytes after the first complete header section are discarded. This lets the
test distinguish one complete request from a split header terminator and
confirm a coalesced second request is never handled. Persistent connections
and body processing require a separate framing layer.

## Build and test

Use a Core installation from this branch, a C++23 compiler, CMake, and Python
3. Build uSockets without its optional TLS and alternate event-loop backends:

```sh
git clone https://github.com/uNetworking/uSockets.git
git -C uSockets checkout 166a04810c9fa970fc22096a7b35b792378364fb
make -C uSockets WITH_LTO=0

cmake -S contrib/http-usockets -B build-usockets \
  -DCMAKE_PREFIX_PATH=/path/to/core-install \
  -DUSOCKETS_ROOT="$PWD/uSockets"
cmake --build build-usockets
ctest --test-dir build-usockets --output-on-failure
```

The test starts the server on an OS-assigned loopback port and owns its
process lifetime. It checks fragmented input with a second connection active,
malformed headers, unsupported body framing, the header size boundary,
coalesced requests, and EOF before the terminating blank line.
