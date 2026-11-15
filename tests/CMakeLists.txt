# Use an installed googletest if there is one (e.g. libgtest-dev on Linux),
# otherwise download it. Release 1.12.1 is the last one that supports C++14
# and older.
find_package(GTest QUIET)
if(NOT GTest_FOUND AND NOT GTEST_FOUND)
    if(CMAKE_VERSION VERSION_LESS 3.14)
        message(WARNING "googletest not found and CMake < 3.14 cannot download it: tests are skipped")
        return()
    endif()
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(TARGET GTest::gtest)
    set(FM_GTEST_TARGET GTest::gtest)
elseif(TARGET GTest::GTest)
    set(FM_GTEST_TARGET GTest::GTest)
else()
    set(FM_GTEST_TARGET gtest)
endif()

add_executable(fm_tests
    main.cpp
    TestUtils.h
    FrameDecoderTest.cpp
    SnapshotDiffTest.cpp
    FileRequestValidatorTest.cpp
    ReconnectPolicyTest.cpp
    FileDownloaderTest.cpp
    FilePreviewTest.cpp
    EventModelTest.cpp
    EventHistoryStoreTest.cpp
    IntegrationTest.cpp)

target_link_libraries(fm_tests PRIVATE fm_server_lib fm_client_core ${FM_GTEST_TARGET})

add_test(NAME fm_tests COMMAND fm_tests)

# UI tests drive the real main window, so they need a display (or
# QT_QPA_PLATFORM=offscreen / xvfb-run).
add_executable(fm_gui_tests
    GuiTestMain.cpp
    TestUtils.h
    MainWindowTest.cpp)

target_link_libraries(fm_gui_tests PRIVATE fm_server_lib fm_client_ui ${FM_GTEST_TARGET})

add_test(NAME fm_gui_tests COMMAND fm_gui_tests)
set_tests_properties(fm_gui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
