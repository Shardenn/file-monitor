# Everything except the widgets lives in a library so the tests can use it.
add_library(fm_client_core STATIC
    EventRecord.h
    EventRecord.cpp
    EventTableModel.h
    EventTableModel.cpp
    EventFilterProxy.h
    EventFilterProxy.cpp
    EventHistoryStore.h
    EventHistoryStore.cpp
    ReconnectPolicy.h
    ReconnectPolicy.cpp
    ServerConnection.h
    ServerConnection.cpp
    FileDownloader.h
    FileDownloader.cpp
    FilePreview.h
    FilePreview.cpp)

target_link_libraries(fm_client_core PUBLIC fm_common)

add_executable(fm_client WIN32
    main.cpp
    MainWindow.h
    MainWindow.cpp
    FilePreviewDialog.h
    FilePreviewDialog.cpp)

target_link_libraries(fm_client PRIVATE fm_client_core Qt5::Widgets Qt5::Concurrent)
