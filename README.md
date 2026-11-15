# File Monitor

A client–server application in C++14 and Qt 5.

- **`fm_server`** is a console program. It watches a directory and tells every
  connected client when a file is created, modified or deleted.
- **`fm_client`** is a Qt Widgets program. It shows the events in a table as
  they happen, downloads and previews files, and can start or stop the
  monitoring on the server.

They talk over TCP with length-prefixed [Protocol Buffers](https://protobuf.dev/)
messages.

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build on Linux](#build-on-linux)
- [Build on Windows](#build-on-windows)
- [Run](#run)
- [Protocol](#protocol)
- [Design](#design)
- [Tests](#tests)
- [Memory checks with valgrind](#memory-checks-with-valgrind)
- [Limitations](#limitations)

## Features

Server:

- Watches one directory (its files, not its subdirectories) with `QFileSystemWatcher`.
- Reports `FILE_CREATED`, `FILE_MODIFIED` and `FILE_DELETED` with the file name,
  full path, size, modification time and SHA-256 checksum.
- Serves any number of clients at once (tested with five and more).
- Sends files on request. Files over 1 MB go in 256 KB chunks and are never
  loaded into memory as a whole.
- Port, directory and listen address are set on the command line.

Client:

- Connects by IP (or host name) and port.
- Event table: time, file name, event type, size. The tooltip of a row shows
  the full path and the checksum.
- Downloads the selected file and shows it: text in a viewer, binary files as
  "No preview available".
- Start/Stop monitoring buttons.
- Shows the connection state and survives a lost connection.

Bonus tasks, all four done:

- Automatic reconnect with exponential backoff (1 s, 2 s, 4 s ... up to 30 s,
  plus jitter). It can be switched off with a checkbox.
- Filtering by event type and by file name (`*` and `?` wildcards).
- Unit tests with Google Test, for the protocol parser and more.
- The event history is saved to a JSON file and loaded on the next start.

## Requirements

| | Version |
|---|---|
| C++ compiler | C++14 (GCC, Clang or MSVC) |
| CMake | 3.10 or newer (3.14 or newer to download googletest automatically) |
| Qt | 5.12 or newer, below 6.0 (Core, Network, Concurrent, Widgets) |
| Protobuf | 3.x (`libprotobuf` and `protoc`) |
| googletest | optional, for the tests |

Tested with:

- Windows 11, MSVC 19.35 (Visual Studio 2022), Qt 5.15.2, Protobuf 3.21.12, CMake 3.24
- Ubuntu 22.04, GCC 11.4, Qt 5.15.3, Protobuf 3.12.4, CMake 3.22

## Build on Linux

Ubuntu or Debian:

```bash
sudo apt install build-essential cmake qtbase5-dev libprotobuf-dev protobuf-compiler libgtest-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build -j
```

The programs are `build/server/fm_server` and `build/client/fm_client`.

## Build on Windows

There are no binary installers for open-source Qt 5.15, and no official
Protobuf binaries for MSVC, so both are built from source once. All commands
run in the **x64 Native Tools Command Prompt for VS 2022** (or 2019).

**1. Qt.** Only the `qtbase` module is needed. Download
[`qtbase-everywhere-src-5.15.2.zip`](https://download.qt.io/archive/qt/5.15/5.15.2/submodules/)
and unpack it to `C:\Qt\src\qtbase`. Then, in an empty folder such as `C:\Qt\build-qtbase`:

```bat
C:\Qt\src\qtbase\configure.bat -prefix C:\Qt\5.15.2\msvc2022_64 -opensource -confirm-license -release -force-debug-info -nomake examples -nomake tests -opengl desktop -no-dbus -no-icu
nmake
nmake install
```

[jom](https://wiki.qt.io/Jom) instead of `nmake` uses all CPU cores and takes
a few minutes instead of an hour.

**2. Protobuf.** Download
[`protobuf-cpp-3.21.12.zip`](https://github.com/protocolbuffers/protobuf/releases/tag/v21.12)
and unpack it to `C:\deps\src`:

```bat
cmake -S C:\deps\src\protobuf-3.21.12 -B C:\deps\build-protobuf -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:\deps\protobuf -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF
cmake --build C:\deps\build-protobuf
cmake --install C:\deps\build-protobuf
```

**3. File Monitor.**

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2022_64;C:/deps/protobuf"
cmake --build build
```

Use `RelWithDebInfo` or `Release`, not `Debug`: the Qt and Protobuf builds
above are release builds, and MSVC does not allow mixing debug and release
runtimes in one program.

The programs need the Qt DLLs at run time. Either add
`C:\Qt\5.15.2\msvc2022_64\bin` to `PATH`, or copy the DLLs next to the
programs:

```bat
C:\Qt\5.15.2\msvc2022_64\bin\windeployqt.exe build\client\fm_client.exe
C:\Qt\5.15.2\msvc2022_64\bin\windeployqt.exe build\server\fm_server.exe
```

googletest is downloaded during configuration. Pass `-DFM_BUILD_TESTS=OFF` to
build without the tests and without network access.

## Run

### Server

```
fm_server [--dir <path>] [--port <port>] [--address <ip>]
```

| Option | Default | Meaning |
|---|---|---|
| `-d`, `--dir` | current directory | directory to watch |
| `-p`, `--port` | `5555` | TCP port to listen on |
| `-a`, `--address` | all interfaces | address to listen on |
| `-h`, `--help` | | show the options |

Example:

```bash
./build/server/fm_server --dir /srv/shared --port 5555
```

The server logs connections and events to the console. Stop it with Ctrl+C.

### Client

```bash
./build/client/fm_client
```

1. Enter the server IP and port and press **Connect**. The coloured line below
   shows the state: connected, connecting, retrying, or the reason of a failure.
2. Events appear in the table. Use the list and the text field above the table
   to filter them.
3. Select a row and press **Download and preview** (or double-click the row).
4. **Start monitoring** / **Stop monitoring** switch the monitoring on the
   server. This affects all clients, and every client shows the new state.

If the connection breaks, the client says so and retries by itself with a
growing delay. **Cancel** stops the retries. With **Auto-reconnect** off, press
**Connect** to reconnect by hand.

The event history is kept between runs in `event_history.json`, in the
per-user application data folder (`%APPDATA%\FileMonitor\FileMonitorClient` on
Windows, `~/.local/share/FileMonitor/FileMonitorClient` on Linux).

## Protocol

### Framing

A TCP connection carries a sequence of frames in each direction:

```
+---------------------+------------------------------+
| length: 4 bytes,    | Envelope: `length` bytes,    |
| unsigned big-endian | serialized protobuf message  |
+---------------------+------------------------------+
```

`length` is the size of the body only. A length over 4 MB is a protocol error,
and so is a body that does not parse as an `Envelope`; the receiver closes the
connection in both cases.

### Messages

The schema is [`proto/filemonitor.proto`](proto/filemonitor.proto). The only
message on the wire is `Envelope`, which holds exactly one of:

| Message | Direction | When |
|---|---|---|
| `ServerHello` | server → client | once, right after the client connects: protocol version, watched directory, whether monitoring is on |
| `FileEvent` | server → all clients | a file was created, modified or deleted |
| `ClientCommand` | client → server | `StartMonitoring`, `StopMonitoring` or `FileRequest`; carries a `request_id` chosen by the client |
| `CommandResult` | server → the requesting client | answer to start/stop, or the reason a file request failed; echoes the `request_id` |
| `MonitoringState` | server → all clients | monitoring was started or stopped (by any client) |
| `FileChunk` | server → the requesting client | one piece of a requested file; echoes the `request_id` |

### File transfer

A successful `FileRequest` is answered with one or more `FileChunk` messages:

- A file of up to 1 MB is one chunk. A larger file is split into 256 KB chunks.
- Every chunk carries `total_size`, its `offset` in the file and `is_last`.
  Chunks arrive in order; an empty file is one chunk without data.
- A failed request (unknown file, name with a path in it, read error) is
  answered with `CommandResult{success = false}` instead. This can also happen
  in the middle of a transfer, if the file changes while it is being sent.

Only plain file names are accepted. A name with a path separator, `..`, or a
symlink that leads out of the watched directory is refused.

### Differences from the example schema in the task

The task gives its schema as an example, so it was adjusted:

- Every enum starts with an `UNSPECIFIED = 0` value. In proto3 the zero value
  is what a missing field reads as; in the example that would be "file
  created" or "start monitoring".
- Commands carry a `request_id` and replies echo it, so a reply can be matched
  to its request.
- The single `ServerResponse` is split into `CommandResult`, `FileChunk` and
  `MonitoringState`, so that each message has only fields that mean something.
- `FileChunk` carries `total_size` and `offset`, for progress and for checking
  that nothing is missing.
- `ServerHello` tells a new client the current state.

Unknown message kinds are ignored by both sides, so the protocol can be
extended without breaking older programs.

## Design

```
proto/    filemonitor.proto          the wire schema
common/   FrameCodec                 length-prefix framing: encodeFrame(), FrameDecoder
          MessageConnection          QTcpSocket <-> stream of Envelopes
server/   DirectoryMonitor           QFileSystemWatcher + snapshot comparison
          SnapshotDiff               what changed between two directory listings
          FileEventCollector         metadata and SHA-256 on a worker thread
          EventQueue                 mutex-protected queue: worker -> network thread
          FileSender                 chunked file sending with backpressure
          FileRequestValidator       keeps file requests inside the directory
          ClientSession              one connected client
          FileMonitorServer          accepts clients, broadcasts, start/stop
client/   ServerConnection           connect, state, reconnect, send and receive
          ReconnectPolicy            exponential backoff
          FileDownloader             chunks -> temporary file
          FilePreview                text or binary? (runs on a worker thread)
          EventTableModel            the table data
          EventFilterProxy           filtering by type and name
          EventHistoryStore          JSON history
          MainWindow, FilePreviewDialog   the widgets
tests/    unit, end-to-end and UI tests
tools/    valgrind script and suppressions
```

**Framing.** `FrameDecoder` has no socket in it: bytes go in, complete
messages come out. It handles a frame split at any byte (also inside the
length prefix) and many frames in one read.

**Threads.** All network I/O is asynchronous, on Qt signals, in the main
thread of each program; nothing waits on a socket. The two slow jobs run on
worker threads:

- Server: hashing files. A single worker thread keeps the events in order and
  hands them over through `EventQueue` (a `QMutex` and a `QQueue`), which
  wakes the network thread with a queued signal.
- Client: reading the downloaded file for the preview (`QtConcurrent`, result
  through `QFutureWatcher`).

No object is shared between threads except that queue.

**Large files.** The server reads a chunk only while the socket's send buffer
is below 1 MB, and continues when the socket reports that bytes were written.
The client writes each chunk straight to a temporary file. The preview reads
at most the first 1 MB. So neither side ever holds a large file in memory.

**Ownership.** Every heap object is owned by a `std::unique_ptr`, a
`std::shared_ptr` or a Qt parent. There is no manual `delete`.

**Detecting changes.** `QFileSystemWatcher` only says that something changed.
The server then lists the directory and compares the listing (name, size,
modification time) with the previous one. Notifications are collected for
100 ms first, so a burst becomes one comparison.

## Tests

```bash
cd build && ctest --output-on-failure
```

Or run the programs directly: `build/tests/fm_tests` and
`build/tests/fm_gui_tests`. On Windows the Qt `bin` folder must be on `PATH`.

- **`fm_tests`**: the frame decoder (byte-by-byte input, every possible split
  point, several frames per read, oversized and corrupt frames), snapshot
  comparison, request validation, backoff, chunk reassembly, text/binary
  detection, the table model, filters and history. It also has end-to-end tests
  with a real server and five real client connections on the loopback
  interface: broadcast of events, a 3 MB chunked download, refused requests,
  start/stop, a client that sends garbage, and reconnect after a server
  restart.
- **`fm_gui_tests`**: drives the real main window against a real server:
  connect, events, filters, text and binary preview, a 5 MB download,
  start/stop, lost connection and reconnect, history. `ctest` runs it without
  a display (`QT_QPA_PLATFORM=offscreen`). Set `FM_SCREENSHOT_DIR` to a folder
  to get screenshots of the window.

## Memory checks with valgrind

On Linux (also inside WSL):

```bash
sudo apt install valgrind xvfb
```

```bash
tools/run_valgrind.sh
```

The script builds the project and runs three things under
`valgrind --leak-check=full`: the server as a real process (a client connects,
files change, the server is stopped with Ctrl+C), `fm_tests`, and
`fm_gui_tests`. It fails if valgrind reports a leak or an invalid memory access.

Two notes for running valgrind by hand:

- The server and `fm_tests` need no suppressions.
- Any Qt GUI program, even an empty one, shows a few small leaks inside
  fontconfig and Qt's platform plugin. [`tools/valgrind.supp`](tools/valgrind.supp)
  hides those two libraries and nothing else:
  `valgrind --leak-check=full --suppressions=tools/valgrind.supp build/client/fm_client`.
  Under WSL also set `QT_XCB_GL_INTEGRATION=none`, otherwise the WSL graphics
  driver adds thousands of reports of its own.

## Limitations

- Only the files directly in the watched directory are monitored, not
  subdirectories.
- Changes made while monitoring is stopped are not reported when it is
  started again.
- A change that keeps both the size and the modification time (to the
  millisecond) of a file is not noticed.
- A rename is reported as a deletion and a creation.
- There is no authentication and no encryption: run it on a trusted network.
- Text preview expects UTF-8. Files in other encodings (for example UTF-16)
  count as binary.
