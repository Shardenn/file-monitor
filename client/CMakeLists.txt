# The logic without widgets is a library, so the tests can use it headless.
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

# The widgets are a library too, so the UI tests can drive the real window.
add_library(fm_client_ui STATIC
    MainWindow.h
    MainWindow.cpp
    FilePreviewDialog.h
    FilePreviewDialog.cpp)

target_link_libraries(fm_client_ui PUBLIC fm_client_core Qt5::Widgets Qt5::Concurrent)

add_executable(fm_client WIN32 main.cpp)

target_link_libraries(fm_client PRIVATE fm_client_ui)
