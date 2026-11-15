protobuf_generate_cpp(FM_PROTO_SRCS FM_PROTO_HDRS
    ${PROJECT_SOURCE_DIR}/proto/filemonitor.proto)

add_library(fm_common STATIC
    Protocol.h
    FrameCodec.h
    FrameCodec.cpp
    MessageConnection.h
    MessageConnection.cpp
    ${FM_PROTO_SRCS}
    ${FM_PROTO_HDRS})

# Generated code is not ours to fix, so its warnings are silenced.
if(MSVC)
    set_source_files_properties(${FM_PROTO_SRCS} PROPERTIES COMPILE_FLAGS /w)
else()
    set_source_files_properties(${FM_PROTO_SRCS} PROPERTIES COMPILE_FLAGS -w)
endif()

target_include_directories(fm_common PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(fm_common PUBLIC
    Qt5::Core
    Qt5::Network
    protobuf::libprotobuf
    Threads::Threads)
