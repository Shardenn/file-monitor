#!/usr/bin/env bash
# Builds the project on Linux and runs the server, the tests and the client
# UI under valgrind. Exits non-zero if valgrind finds a leak or an invalid
# memory access in any of them.
#
# Usage: tools/run_valgrind.sh [build-dir]        (default: build-valgrind)
#
# Needs: cmake, a C++14 compiler, Qt 5, protobuf, googletest, valgrind and
# xvfb. On Ubuntu:
#   sudo apt install build-essential cmake qtbase5-dev libprotobuf-dev \
#        protobuf-compiler libgtest-dev valgrind xvfb
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$SOURCE_DIR/build-valgrind}"
SUPPRESSIONS="$SOURCE_DIR/tools/valgrind.supp"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR" -j"$(nproc)"

VALGRIND=(valgrind
    --leak-check=full
    --show-leak-kinds=definite,indirect
    --errors-for-leak-kinds=definite,indirect
    --error-exitcode=9
    --suppressions="$SUPPRESSIONS")

failed=0
run() {
    local name="$1"
    shift
    echo
    echo "=== $name ==="
    if "$@"; then
        echo "=== $name: clean ==="
    else
        echo "=== $name: FAILED (exit code $?) ==="
        failed=1
    fi
}

# The server as a real process: one client connects, a file changes, then
# the server is stopped with SIGINT so that it shuts down normally.
server_session() {
    local dir port=5599 status=0
    dir="$(mktemp -d)"
    "${VALGRIND[@]}" "$BUILD_DIR/server/fm_server" --dir "$dir" --port "$port" &
    local pid=$!
    sleep 5
    # Hold a connection open while files change, so events are sent to it.
    (exec 3<>"/dev/tcp/127.0.0.1/$port"; sleep 4) &
    sleep 1
    echo "hello" >"$dir/a.txt"
    sleep 1
    echo "more" >>"$dir/a.txt"
    rm "$dir/a.txt"
    sleep 3
    kill -INT "$pid"
    wait "$pid" || status=$?
    rm -rf "$dir"
    return "$status"
}

run "server" server_session
run "unit and integration tests" "${VALGRIND[@]}" "$BUILD_DIR/tests/fm_tests"
# The UI does not use OpenGL. Not loading the GL driver keeps its (large)
# valgrind noise out of the report; this matters most under WSL.
export QT_XCB_GL_INTEGRATION=none
run "client UI tests" xvfb-run -a "${VALGRIND[@]}" "$BUILD_DIR/tests/fm_gui_tests"

echo
if [ "$failed" -eq 0 ]; then
    echo "All valgrind runs are clean."
else
    echo "Some valgrind runs failed, see above."
fi
exit "$failed"
