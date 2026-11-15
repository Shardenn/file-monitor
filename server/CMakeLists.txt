add_library(fm_server_lib STATIC
    SnapshotDiff.h
    SnapshotDiff.cpp
    DirectoryMonitor.h
    DirectoryMonitor.cpp
    EventQueue.h
    EventQueue.cpp
    FileEventCollector.h
    FileEventCollector.cpp
    FileRequestValidator.h
    FileRequestValidator.cpp
    FileSender.h
    FileSender.cpp
    ClientSession.h
    ClientSession.cpp
    FileMonitorServer.h
    FileMonitorServer.cpp)

target_link_libraries(fm_server_lib PUBLIC fm_common Qt5::Concurrent)

add_executable(fm_server
    main.cpp
    ShutdownSignal.h
    ShutdownSignal.cpp)

target_link_libraries(fm_server PRIVATE fm_server_lib)
