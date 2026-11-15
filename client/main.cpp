#include <QApplication>

#include "client/EventHistoryStore.h"
#include "client/MainWindow.h"
#include "common/Protocol.h"

int main(int argc, char* argv[])
{
    int exitCode = 0;
    {
        QApplication app(argc, argv);
        QApplication::setApplicationName(QStringLiteral("FileMonitorClient"));
        QApplication::setOrganizationName(QStringLiteral("FileMonitor"));

        fm::MainWindow window(fm::EventHistoryStore::defaultFilePath());
        window.show();
        exitCode = app.exec();
    }

    // Frees protobuf's global tables so leak checkers report a clean exit.
    google::protobuf::ShutdownProtobufLibrary();
    return exitCode;
}
