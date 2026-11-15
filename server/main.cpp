#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

#include "common/Protocol.h"
#include "server/FileMonitorServer.h"
#include "server/ShutdownSignal.h"

namespace {

int runServer(QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("File Monitor server: watches a directory and publishes file events."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption dirOption(
        {QStringLiteral("d"), QStringLiteral("dir")},
        QStringLiteral("Directory to watch (default: current directory)."),
        QStringLiteral("path"), QStringLiteral("."));
    const QCommandLineOption portOption(
        {QStringLiteral("p"), QStringLiteral("port")},
        QStringLiteral("TCP port to listen on (default: %1).").arg(fm::kDefaultPort),
        QStringLiteral("port"), QString::number(fm::kDefaultPort));
    const QCommandLineOption addressOption(
        {QStringLiteral("a"), QStringLiteral("address")},
        QStringLiteral("Address to listen on (default: all interfaces)."),
        QStringLiteral("ip"));
    parser.addOptions({dirOption, portOption, addressOption});
    parser.process(app);

    const QFileInfo dirInfo(parser.value(dirOption));
    if (!dirInfo.isDir()) {
        qCritical().noquote() << "Not a directory:" << parser.value(dirOption);
        return 1;
    }
    const QString directory = dirInfo.canonicalFilePath();

    bool portOk = false;
    const uint port = parser.value(portOption).toUInt(&portOk);
    if (!portOk || port > 65535) {
        qCritical().noquote() << "Invalid port:" << parser.value(portOption);
        return 1;
    }

    QHostAddress address(QHostAddress::Any);
    if (parser.isSet(addressOption) && !address.setAddress(parser.value(addressOption))) {
        qCritical().noquote() << "Invalid address:" << parser.value(addressOption);
        return 1;
    }

    fm::FileMonitorServer server(directory);
    if (!server.listen(address, static_cast<quint16>(port))) {
        qCritical().noquote() << "Cannot listen on port" << port << ':' << server.errorString();
        return 1;
    }
    qInfo().noquote() << "Watching" << QDir::toNativeSeparators(directory);
    qInfo().noquote() << "Listening on port" << server.port() << "- press Ctrl+C to stop";

    fm::ShutdownSignal shutdownSignal;
    QObject::connect(&shutdownSignal, &fm::ShutdownSignal::requested, &app, [&app] {
        qInfo() << "Shutting down";
        app.quit();
    });

    return app.exec();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fm_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    const int exitCode = runServer(app);

    // Frees protobuf's global tables so leak checkers report a clean exit.
    google::protobuf::ShutdownProtobufLibrary();
    return exitCode;
}
