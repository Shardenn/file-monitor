#include <QApplication>

#include <gtest/gtest.h>

#include "common/Protocol.h"

int main(int argc, char* argv[])
{
    int result = 0;
    {
        QApplication app(argc, argv);
        ::testing::InitGoogleTest(&argc, argv);
        result = RUN_ALL_TESTS();
    }
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}
