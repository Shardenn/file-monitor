# File Monitor — Implementation Plan

Client–server "File Monitor" in **C++14 / Qt 5.15.2 / Protobuf 3.x / CMake ≥ 3.10**, based on `task.pdf`.
All code, comments, commits and docs are in English.

---

## 1. Requirements summary

### Server (console app)
- Watches a directory (`QFileSystemWatcher`) and emits `FILE_CREATED`, `FILE_MODIFIED`, `FILE_DELETED`.
- For each event it collects: name, full path, size, modification time (unix), checksum (**SHA-256**).
- Listens on TCP; the port comes from the CLI (`--port`). It broadcasts events to all clients and supports at least 5 concurrent clients.
- Handles the client commands `MONITORING_START`, `MONITORING_STOP` and `FILE_REQUEST`.

### Client (Qt Widgets GUI)
- IP and port input fields with Connect and Disconnect buttons, plus a connection state indicator.
- Live event table (`QTableView` + custom model) with these columns: time, file name, event type, size.
- Download a selected file. Text files open in a viewer. Binary files show "No preview available".
- Start/Stop monitoring buttons.
- Handles a dropped connection: it shows the state and allows reconnecting.

### Protocol
- **Mandatory:** a `.proto` schema with **at least 3 messages**. The schema in the task is only an example, so we design our own (see §4.0). It is informed by the example but fixes its weak points.
- Framing: a 4-byte **big-endian** length prefix, then the serialized `Envelope`.
- Files larger than 1 MB are sent in chunks and are never loaded fully into memory.

### Non-functional (graded)
| Criterion | Weight | How we address it |
|---|---|---|
| Protocol correctness: prefix parsing, reassembly of fragmented packets, partial messages | high | A standalone `FrameDecoder` class (no sockets), covered by unit tests: byte-by-byte feed, several frames in one buffer, split prefix, oversize/garbage frames |
| Threads: no UI freezes, no races | high | Network I/O is fully async via signals and slots. Hashing and file reading run on worker threads (`QThreadPool`/`QtConcurrent`), and results return through queued signals. Shared queues are guarded by a `QMutex` |
| Memory leaks and dangling pointers (valgrind) | high | RAII, `std::unique_ptr`/`std::shared_ptr`, Qt parent ownership, `deleteLater()` for sockets, no raw `new` without an owner. Valgrind is run on Linux (WSL) |
| Error handling: unreachable server, broken connection, reconnect | medium | Client state machine plus auto-reconnect with exponential backoff |
| Readability: naming, file structure, no god classes | medium | Small single-purpose classes (see §4) |
| README with build and run instructions | mandatory | `README.md` covering build, run and protocol description |

### Bonus tasks (all planned)
1. Auto-reconnect with exponential backoff (1s → 2s → 4s … capped at 30s, with jitter).
2. Event filtering in the GUI by type and file name (`QSortFilterProxyModel`).
3. Unit tests for the protocol parser (Google Test).
4. Event history saved to disk as **JSON** (client side, on exit and on load). This avoids pulling in the SQLite plugin.

---

## 2. Toolchain and environment (Windows 11 host)

Already installed: **CMake 3.24**, **Visual Studio 2022 (MSVC 14.35)** and **VS 2019 (MSVC 14.29)**, git, Python 3.11.
Missing: Qt, Protobuf, Ninja.

### 2.0 Environment status (set up 2026-11-15) ✅
| Component | Version | Location |
|---|---|---|
| Compiler | **MSVC 19.35 (VS 2022, v143 toolset)** | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| Qt (qtbase, built from source) | **5.15.2**, release + PDBs, shared | `C:\Qt\5.15.2\msvc2022_64` (source in `C:\Qt\src\qtbase`, build tree in `C:\Qt\build-qtbase`) |
| Protobuf (built from source) | **3.21.12**, static lib, `/MD` runtime | `C:\deps\protobuf` (`bin\protoc.exe`, CMake config in `C:\deps\protobuf\cmake`) |
| Build driver | jom 1.1.x (no Ninja installed) | `C:\Qt\jom\jom.exe` |
| CMake | 3.24.1 | system |

A CMake test project (Qt Core/Network/Widgets/Concurrent + `protobuf_generate_cpp`, C++14) was configured, built and run successfully. It used `QTcpServer::listen`, `QtConcurrent::run`, a `QLabel` on the offscreen platform, and a protobuf round-trip.

**How to configure the project** (from a VS 2022 x64 environment, `vcvars64.bat`):
```
cmake -S . -B build -G "NMake Makefiles JOM" -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2022_64;C:/deps/protobuf"
cmake --build build
```
All programs are built into `buildin`, and the build copies the Qt DLLs and plugins there (`FM_DEPLOY_QT`), so they run without Qt on `PATH`. This Qt build has no `windeployqt` (it is part of qttools, not qtbase), so CMake does the copying itself.

### 2.1 Qt 5.15.2
The archive at `https://download.qt.io/archive/qt/5.15/5.15.2/` has **only source packages**, because open-source offline installers stopped with 5.15. qtbase was built from `submodules/qtbase-everywhere-src-5.15.2.zip`:
```
configure -prefix C:\Qt\5.15.2\msvc2022_64 -opensource -confirm-license -release -force-debug-info ^
          -nomake examples -nomake tests -opengl desktop -no-dbus -no-icu -platform win32-msvc
jom && jom install
```
The build and install took about 3–4 minutes on 32 threads (measured from file timestamps), with 22 harmless compiler warnings and no errors. Installed modules include Core, Gui, Widgets, Network, Concurrent, Test, Sql (with the SQLite driver) and Xml.

### 2.2 Protobuf 3.x
- Built **protobuf 3.21.12** (the last 3.x release, no abseil) with `-Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF`. It is a static library linked against the dynamic CRT, so it matches Qt's `/MD`.
- In CMake we use `find_package(Protobuf REQUIRED)` and `protobuf_generate_cpp()`. Both were verified to work.

### 2.3 Google Test
- Pulled with CMake `FetchContent` (release-1.12.1, the last version that supports C++11/14). No system install is needed.
- **We must set `gtest_force_shared_crt ON`** so gtest uses `/MD` like Qt and protobuf.

### 2.4 Linux / valgrind check (WSL) ✅
- **Installed:** WSL distribution **Ubuntu-22.04** (no default user; commands run as `wsl -d Ubuntu-22.04 -u root`). Packages: `build-essential cmake qtbase5-dev libprotobuf-dev protobuf-compiler valgrind xvfb git`.
- **Versions:** GCC 11.4, CMake 3.22.1, Qt 5.15.3, protoc/libprotobuf 3.12.4, valgrind 3.18.1. All are within the task's ranges (CMake ≥ 3.10, 5.12 ≤ Qt < 6, Protobuf 3.x).
- **Check:** the same CMake test project builds and runs on Linux unchanged.
- **Build location:** build inside the Linux filesystem (e.g. copy or clone to `/tmp` or `~`), not on `/mnt/c`, for speed and correct permissions.
- **Running under valgrind:** the server and tests run under `valgrind --leak-check=full --error-exitcode=9`. The client runs under `xvfb-run` (the real `xcb` platform). The `offscreen` platform is only for quick checks.
- **Compatibility:** the code must build with **protobuf 3.12** (no 3.21-only APIs), and must use no Windows-only APIs.
- **Finding: third-party leak noise.** Even the minimal test reports 2 "definitely lost" blocks (~300 bytes). They come from libfontconfig/expat (font config parsing) and Qt's `libqoffscreen.so` platform plugin, not our code. So we add **`tools/valgrind.supp`**, which suppresses only these known library stacks, matched by object name (`obj:*libfontconfig*`, `obj:*/platforms/libq*.so`). Our acceptance criterion is "definitely/indirectly lost: 0" with the suppressions applied. The server (`QCoreApplication`, no GUI libraries) should be clean even without them.
- **Script:** `tools/run_valgrind.sh` builds in WSL and runs server, tests and client with the suppressions. Both files are documented in the README.

### 2.5 Build findings that change the plan
1. **Compiler: VS 2022 (v143) instead of VS 2019 (v142).** The VS 2019 folder holds only an empty toolset stub, with no `cl.exe`. v143 is ABI-compatible with v142, and Qt 5.15.2 built cleanly with it. The Qt prefix is `msvc2022_64`, not `msvc2019_64`.
2. **Qt and protobuf are release-only (`/MD`).** A CMake `Debug` build of our code would use `/MDd` and mix CRTs with the Qt DLLs, which is unsafe. So we use **`RelWithDebInfo`** for day-to-day work. It is fully debuggable, since Qt was built with `-force-debug-info` and PDBs are installed. A debug Qt build can be added later if it's really needed (a few minutes).
3. **No Ninja.** We use the `NMake Makefiles JOM` generator. Ninja can be added later; nothing depends on it.
4. **Protobuf's CMake config lives in `<prefix>\cmake`** (the Windows layout). `CMAKE_PREFIX_PATH` must include `C:/deps/protobuf`. This is documented in the README.
5. **The README must cover the Windows source-build route** for Qt (because there are no binary installers) as well as the simple `apt` route on Linux.

---

## 3. Repository layout

```
sozv/
├── CMakeLists.txt              # top level: options, C++14, subdirs
├── cmake/                      # helper modules (if needed)
├── proto/
│   └── filemonitor.proto
├── common/                     # static lib "fm_common"
│   ├── CMakeLists.txt          # protobuf_generate_cpp + lib
│   ├── FrameCodec.h/.cpp       # encode(Envelope) -> QByteArray; FrameDecoder (stateful)
│   ├── MessageConnection.h/.cpp# QTcpSocket wrapper: framing + Envelope signals
│   └── Constants.h             # chunk size (256 KB), 1 MB threshold, max frame size
├── server/
│   ├── CMakeLists.txt
│   ├── main.cpp                # CLI parsing (QCommandLineParser): --dir, --port
│   ├── DirectoryMonitor.h/.cpp # QFileSystemWatcher + snapshot diffing -> FileChange
│   ├── FileInfoCollector.h/.cpp# metadata + SHA-256 on worker thread (QCryptographicHash, streamed)
│   ├── EventQueue.h/.cpp       # thread-safe queue (QMutex + QQueue) + signal
│   ├── ClientSession.h/.cpp    # one per client: owns MessageConnection, handles commands
│   ├── FileTransfer.h/.cpp     # chunked file sending, paced by socket bytesWritten
│   └── FileMonitorServer.h/.cpp# QTcpServer, session registry, broadcast
├── client/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── MainWindow.h/.cpp/.ui   # layout only; delegates to components below
│   ├── ServerConnection.h/.cpp # state machine + reconnect with backoff
│   ├── EventTableModel.h/.cpp  # QAbstractTableModel over std::vector<EventRecord>
│   ├── EventFilterProxy.h/.cpp # filter by type + name
│   ├── FileDownloader.h/.cpp   # reassembles chunks into a temp file (QTemporaryFile)
│   ├── FilePreviewDialog.h/.cpp# text viewer / "No preview available"
│   └── EventHistoryStore.h/.cpp# JSON load/save
├── tools/
│   ├── valgrind.supp           # suppressions for known Qt/fontconfig library leaks
│   └── run_valgrind.sh         # WSL: build + run server/tests/client under valgrind
├── tests/
│   ├── CMakeLists.txt          # FetchContent googletest
│   ├── FrameDecoderTest.cpp
│   └── EventTableModelTest.cpp # (optional)
├── PLAN.md
├── README.md
└── .gitignore
```

---

## 4. Design details

### 4.0 Protobuf schema (own design)

Problems with the example schema that we fix:
- **No "unspecified" default.** In proto3 every enum's zero value is the default. In the example `FILE_CREATED = 0` and `MONITORING_START = 0`, so a missing or garbled field silently reads as "created" or "start". Every enum here gets an `*_UNSPECIFIED = 0`, and the receiver rejects it.
- **No request correlation.** Several clients can send commands and several downloads can be queued, so every command carries a `request_id`. Every reply to it echoes that id.
- **One overloaded response message.** `ServerResponse` mixes status, file data and errors in a single shape. We split it into dedicated messages, so each one has only meaningful fields.
- **Missing context for downloads.** The client can't show progress or check completeness without the total size and the offset. File chunks carry `total_size` and `offset`.
- **Client doesn't know initial state.** On connect the server sends a `ServerHello` with the protocol version, the watched directory and whether monitoring is active.
- **Ambiguous timestamp.** Unix time in **milliseconds** (`mtime_ms`), with the unit in the field name.

```proto
syntax = "proto3";
package filemonitor.proto;

enum EventType {
  EVENT_TYPE_UNSPECIFIED = 0;
  FILE_CREATED  = 1;
  FILE_MODIFIED = 2;
  FILE_DELETED  = 3;
}

// Server -> client, once right after the connection is accepted.
message ServerHello {
  uint32 protocol_version  = 1;
  string watched_directory = 2;
  bool   monitoring_active = 3;
}

// Server -> all clients.
message FileEvent {
  EventType type      = 1;
  string    file_name = 2;
  string    file_path = 3;
  uint64    size      = 4;   // 0 for FILE_DELETED
  int64     mtime_ms  = 5;   // unix time, ms; event time for FILE_DELETED
  bytes     sha256    = 6;   // empty for FILE_DELETED or if the file was unreadable
}

// Client -> server.
message ClientCommand {
  uint64 request_id = 1;     // chosen by the client, echoed in replies
  oneof action {
    StartMonitoring start_monitoring = 2;
    StopMonitoring  stop_monitoring  = 3;
    FileRequest     file_request     = 4;
  }
}
message StartMonitoring {}
message StopMonitoring  {}
message FileRequest { string file_name = 1; }   // name relative to the watched dir

// Server -> requesting client: result of start/stop (or a refused request).
message CommandResult {
  uint64 request_id = 1;
  bool   success    = 2;
  string message    = 3;     // human-readable status or error text
}

// Server -> all clients whenever monitoring is started or stopped.
message MonitoringState { bool active = 1; }

// Server -> requesting client: one piece of a file.
message FileChunk {
  uint64 request_id  = 1;
  string file_name   = 2;
  uint64 total_size  = 3;
  uint64 offset      = 4;
  bytes  data        = 5;    // <= 256 KB
  bool   is_last     = 6;
}

// The only top-level message on the wire (length-prefixed).
message Envelope {
  oneof body {
    ServerHello     hello            = 1;
    FileEvent       file_event       = 2;
    ClientCommand   command          = 3;
    CommandResult   command_result   = 4;
    MonitoringState monitoring_state = 5;
    FileChunk       file_chunk       = 6;
  }
}
```

Rules:
- A failed `FileRequest` (not found, outside the watched directory, read error) is answered with `CommandResult{success=false}` carrying the same `request_id`. A successful one is answered with a stream of `FileChunk`s.
- A frame whose `Envelope.body` is unset, or whose enum is `*_UNSPECIFIED`, is a protocol error. The receiver logs it and drops the frame. Malformed framing closes the connection.
- `protocol_version` starts at 1. The client warns on a mismatch.
- Field numbers are never reused, which keeps future changes compatible.

### 4.1 Framing (`common/FrameCodec`)
- `QByteArray encodeFrame(const fm::Envelope&)` writes a 4-byte BE length (`qToBigEndian<quint32>`) followed by `SerializeToArray`.
- `class FrameDecoder`:
  - `void append(const QByteArray&)` adds bytes to an internal buffer.
  - `bool next(fm::Envelope& out)` returns one frame when a complete one is buffered, so it handles partial prefixes, partial bodies and several frames per read.
  - `Status` reports `NeedMoreData`, `Ok` or `Error`. An error is a length above `kMaxFrameSize` (e.g. 4 MB) or a failed parse. On error the connection is closed.
  - The buffer is compacted with an offset so it is not re-copied on every frame.
- The class has no Qt network dependency, so it is fully unit-testable.

### 4.2 `MessageConnection` (shared by client and server)
- Wraps a `QTcpSocket` owned through a parent or `unique_ptr`. On `readyRead` it feeds the decoder and emits `envelopeReceived(Envelope)` for each frame.
- `send(const Envelope&)` writes asynchronously. `bytesToWrite()` is exposed for backpressure.
- Its signals are `disconnected` and `errorOccurred(QString)`.

### 4.3 Server
- **DirectoryMonitor**: `QFileSystemWatcher` only reports "directory changed" and "file changed". On `directoryChanged` it diffs a snapshot (`std::unordered_map<QString, Snapshot{size, mtime}>`) to produce created and deleted events. Modifications come from `fileChanged` plus size/mtime comparison. The watch is non-recursive (top-level directory, as the task says). A short debounce timer (~100 ms) coalesces bursts.
- **FileInfoCollector**: runs in `QThreadPool` and reads files in 64 KB blocks into `QCryptographicHash::Sha256`. Large files are never fully loaded. Results go into **EventQueue** (`QMutex` + `QQueue<FileEvent>`), which emits `eventsAvailable()` as a queued signal. The main thread drains the queue and broadcasts.
- **FileMonitorServer**: a `QTcpServer` with a `std::vector<std::unique_ptr<ClientSession>>` (or a parent-owned list). On disconnect a session is removed with `deleteLater` semantics, with no dangling pointers inside broadcast loops (`std::remove_if` after the loop). There is no hard cap on clients; at least 5 are tested.
- **On connect**: the session sends a `ServerHello` first.
- **Commands**: `StartMonitoring`/`StopMonitoring` toggle the monitor. The requesting client gets a `CommandResult`, and every client gets a `MonitoringState` broadcast. `FileRequest` validates the file name (no `..`, no absolute paths, must resolve inside the watched directory) and starts a `FileTransfer`. Failures produce `CommandResult{success=false}`.
- **FileTransfer**: opens a `QFile` and sends one `FileChunk` (256 KB) at a time. The next chunk is read only when the socket's `bytesToWrite()` falls below a threshold (driven by `bytesWritten`). This keeps memory bounded and the event loop responsive. Files ≤ 1 MB go as a single chunk (`offset=0`, `is_last=true`). There is one active transfer per session, and later requests are queued.

### 4.4 Client
- **ServerConnection** states: `Disconnected → Connecting → Connected → (Reconnecting)`. It emits `stateChanged`, `eventReceived`, `responseReceived`. Auto-reconnect uses a `QTimer` with backoff `min(30s, 1s·2^n)` plus jitter. It resets on a successful connect and is cancelled by a manual Disconnect. Connection timeout is 5 s.
- **EventTableModel**: `std::vector<EventRecord>` with a row cap (e.g. 10,000, oldest dropped) so memory stays bounded. Rows are appended with `beginInsertRows`.
- **EventFilterProxy**: a type combo box (All/Created/Modified/Deleted) and a name `QLineEdit` with a wildcard/substring match.
- **Request tracking**: `ServerConnection` hands out increasing `request_id`s and keeps the pending requests in a `std::unordered_map<uint64_t, PendingRequest>`, so each `CommandResult` or `FileChunk` is routed to whoever asked. Pending requests are failed when the connection drops.
- **Monitoring state**: comes from `ServerHello` and `MonitoringState`, and drives the Start/Stop buttons and a state label.
- **FileDownloader**: writes `FileChunk`s to a `QTemporaryFile` and checks that each `offset` matches the bytes received so far. It shows progress from `total_size`. When the transfer finishes, the file is classified as text or binary on a worker thread (`QtConcurrent::run`: look for NUL bytes or invalid UTF-8 in the first 8 KB). Text previews are capped (e.g. the first 2 MB, with a "truncated" note). The result is shown in **FilePreviewDialog**.
- **EventHistoryStore**: JSON array in `QStandardPaths::AppDataLocation`. It loads on start and saves on exit, with a "Clear history" action.
- **MainWindow** handles only UI wiring: a connection bar (IP, port, Connect/Disconnect, status label), a monitoring bar (Start/Stop), filters, the table, and a "Download / Preview" button (also triggered by double-clicking a row).

### 4.5 Memory and thread-safety rules
- Every heap object has a clear owner: a Qt parent or a smart pointer. No raw owning pointers.
- Objects never cross threads except value types (`FileEvent` is copied, and protobuf/Qt value types are registered with `qRegisterMetaType`).
- Worker lambdas capture by value and report back through `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` against a `QPointer`-guarded receiver.
- Shutdown is clean: stop the watcher, `QThreadPool::waitForDone()`, then close sessions. `google::protobuf::ShutdownProtobufLibrary()` runs at exit so valgrind output stays clean.

---

## 5. Testing strategy
- **Unit tests (GTest)**:
  - Frame decoder cases: a single full frame, byte-by-byte feed, multiple frames in one chunk, a prefix split across reads, a zero-length frame, an oversized length, and a corrupt payload.
  - Encode→decode round-trip for each `oneof` variant. Rejection of an unset `body` and of `*_UNSPECIFIED` enums.
  - Snapshot diff logic of `DirectoryMonitor`, extracted as a pure function.
  - Chunk reassembly in `FileDownloader`.
- **Integration (manual, scripted)**:
  - Start the server and connect 5+ clients (GUI, or a small headless test client in `tests/`).
  - Create, modify and delete files; download a small text file, a binary file and a file of 50 MB or more.
  - Kill the server to verify the client shows "Disconnected" and then reconnects with backoff.
- **Valgrind** runs on WSL for the server, the tests and the client.

---

## 6. Milestones and commit history
The task requires real history, not one giant commit. Planned commits:
1. `chore: project skeleton, CMake, .gitignore, PLAN.md`
2. `feat(proto): protocol schema and protobuf code generation`
3. `feat(common): length-prefixed frame codec` + `test: frame decoder unit tests`
4. `feat(common): MessageConnection socket wrapper`
5. `feat(server): directory monitor with snapshot diffing`
6. `feat(server): async metadata + SHA-256 collection, thread-safe event queue`
7. `feat(server): TCP server, client sessions, event broadcast`
8. `feat(server): monitoring start/stop commands`
9. `feat(server): chunked file transfer with backpressure`
10. `feat(client): main window, connection handling`
11. `feat(client): event table model`
12. `feat(client): file download and preview`
13. `feat(client): auto-reconnect with exponential backoff`
14. `feat(client): filtering by type and name`
15. `feat(client): JSON event history`
16. `docs: README with build/run instructions and protocol description`
17. `fix:` items found in valgrind and integration testing

---

## 7. Execution order (next steps)
1. ~~Set up the environment~~ ✅ done: the Windows toolchain (§2.0) and WSL Ubuntu 22.04 with valgrind (§2.4).
2. `git init`, then milestones 1–4. Unit tests must pass before any networking work.
3. Server milestones 5–9, tested with a minimal client or the tests.
4. Client milestones 10–15.
5. Integration test with 5+ clients and large files; valgrind on WSL.
6. README and final cleanup.

## 8. Open questions / assumptions
- Monitoring is **non-recursive** (top-level files of the given directory). Recursive watching can be an option later.
- The checksum is **SHA-256**.
- The protobuf schema is our own design (§4.0). The task's schema is only an example; the mandatory part is having at least 3 messages, and ours has 11, including `Envelope`.
- History storage is **JSON** on the client side.
- The server keeps no auth or TLS (out of scope).
- Default port is `5555`. The default directory is the current directory unless `--dir` is given.

---

## 9. Implementation status (2026-11-15) ✅

Everything in this plan is implemented, including the four bonus tasks.

**Verified:**
- Windows (MSVC 19.35, Qt 5.15.2, Protobuf 3.21.12): builds with no warnings; `fm_tests` 68/68 and `fm_gui_tests` 8/8 pass.
- Ubuntu 22.04 in WSL (GCC 11.4, Qt 5.15.3, Protobuf 3.12.4): builds with `-Wall -Wextra` and no warnings; `fm_tests` 69/69 (one more test, for symlinks) and `fm_gui_tests` 8/8 pass.
- `tools/run_valgrind.sh`: the server process, `fm_tests` and `fm_gui_tests` all finish with 0 errors and 0 bytes definitely or indirectly lost.

**Where the code differs from the plan above:**
1. **No `.ui` file.** The window is built in code (`MainWindow.cpp`), which keeps the build free of `uic`.
2. **Request tracking is simpler.** `ServerConnection` only hands out request ids. `FileDownloader` remembers the id of its download and ignores replies for other ids. A separate map of pending requests turned out to be unnecessary with one download at a time.
3. **Session ownership.** The server holds sessions in `std::vector<std::unique_ptr<ClientSession>>`. A session reports its closing through a *queued* signal, so it is never destroyed inside one of its own signal handlers. No `deleteLater()` is needed.
4. **Extra libraries for testing.** The server logic (`fm_server_lib`), the client logic (`fm_client_core`) and the widgets (`fm_client_ui`) are static libraries. `fm_tests` and the new **`fm_gui_tests`** link them and drive the real server and the real window.
5. **Name filter without regular expressions.** The first version used `QRegularExpression`; its JIT compiler makes valgrind report invalid reads (a known false alarm). A 30-line wildcard matcher replaced it.
6. **Server logging.** On Windows, Qt drops log messages when stderr is redirected, so the server installs a handler that always writes to stderr with a timestamp.
7. **Clean shutdown.** `ShutdownSignal` turns Ctrl+C (SIGINT/SIGTERM) into a normal exit, which valgrind needs to give a meaningful leak report for the server.
8. **CMake on Windows.** CMake's `FindProtobuf` assumes a DLL under MSVC, so the top-level `CMakeLists.txt` sets `Protobuf_USE_STATIC_LIBS` by default there.
9. **Valgrind noise found on the way.** Besides fontconfig and the Qt platform plugin (suppressed in `tools/valgrind.supp`), the WSL graphics driver produces thousands of reports unless `QT_XCB_GL_INTEGRATION=none` is set. The script sets it.

10. **Programs in `build/bin` with Qt next to them (Windows).** Found when a second client was started by hand: `PATH` on this machine contains MiKTeX's own `Qt5Core.dll`, which the client loaded instead of ours and then failed with "no Qt platform plugin could be initialized". The build now copies the right DLLs and plugins next to the programs.

**Not verified:**
- Ctrl+C handling of the server on a real Windows console (it is verified on Linux, and the same code path is used).
- The client was exercised through `fm_gui_tests` and screenshots, not by hand with a mouse.
- The intermediate git commits were made after the whole tree built and passed; they are split by component, and the early ones do not build on their own.
