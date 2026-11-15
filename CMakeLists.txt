cmake_minimum_required(VERSION 3.10)
project(FileMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(FM_BUILD_TESTS "Build unit and integration tests" ON)

# With MSVC, CMake assumes Protobuf is a DLL unless told otherwise, which
# breaks linking against the usual static build. Pass
# -DProtobuf_USE_STATIC_LIBS=OFF if your Protobuf really is a DLL.
if(MSVC AND NOT DEFINED Protobuf_USE_STATIC_LIBS)
    set(Protobuf_USE_STATIC_LIBS ON)
endif()

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Network Concurrent Widgets)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    # Qt and Protobuf are usually release-only builds on Windows (/MD).
    # googletest must use the same runtime.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

if(FM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
