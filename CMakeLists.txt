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

# All programs go to one folder, so on Windows one copy of the Qt DLLs
# serves all of them.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

# On Windows a program loads the first Qt5Core.dll it finds on PATH, and
# other software (MiKTeX, Python packages, ...) often puts its own copy
# there. With the right DLLs and plugins next to the programs they start
# from anywhere, whatever PATH says.
if(WIN32)
    option(FM_DEPLOY_QT "Copy the Qt DLLs and plugins next to the programs" ON)
    if(FM_DEPLOY_QT)
        set(FM_BIN_DIR $<TARGET_FILE_DIR:fm_client>)
        add_custom_target(fm_deploy_qt ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt5::Core>
                $<TARGET_FILE:Qt5::Network>
                $<TARGET_FILE:Qt5::Concurrent>
                $<TARGET_FILE:Qt5::Gui>
                $<TARGET_FILE:Qt5::Widgets>
                ${FM_BIN_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${FM_BIN_DIR}/platforms ${FM_BIN_DIR}/styles
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt5::QWindowsIntegrationPlugin> ${FM_BIN_DIR}/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt5::QOffscreenIntegrationPlugin> ${FM_BIN_DIR}/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt5::QWindowsVistaStylePlugin> ${FM_BIN_DIR}/styles
            COMMENT "Copying the Qt DLLs and plugins next to the programs"
            VERBATIM)
        add_dependencies(fm_deploy_qt fm_client)
    endif()
endif()

if(FM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
