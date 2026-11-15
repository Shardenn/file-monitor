#include <QCoreApplication>

#include <gtest/gtest.h>

#include "common/Protocol.h"

int main(int argc, char* argv[])
{
    int result = 0;
    {
        // Sockets, timers and file watchers need an application object.
        QCoreApplication app(argc, argv);
        ::testing::InitGoogleTest(&argc, argv);
        result = RUN_ALL_TESTS();
    }
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}
