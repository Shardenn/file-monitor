#include "server/FileMonitorServer.h"

#include <QLoggingCategory>

#include <algorithm>

namespace fm {

namespace {

const char* eventTypeName(pb::EventType type)
{
    switch (type) {
    case pb::FILE_CREATED:
        return "created";
    case pb::FILE_MODIFIED:
        return "modified";
    case pb::FILE_DELETED:
        return "deleted";
    default:
        return "unknown";
    }
}

} // namespace

FileMonitorServer::FileMonitorServer(const QString& directory, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
    , m_monitor(directory)
    , m_collector(directory)
{
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &FileMonitorServer::onNewConnection);
    connect(&m_monitor, &DirectoryMonitor::changesDetected, &m_collector,
            &FileEventCollector::submit);
    connect(&m_collector, &FileEventCollector::eventsReady, this,
            &FileMonitorServer::broadcastReadyEvents);
    connect(&m_monitor, &DirectoryMonitor::activeChanged, this, [this](bool active) {
        qInfo() << "Monitoring" << (active ? "started" : "stopped");
        pb::Envelope envelope;
        envelope.mutable_monitoring_state()->set_active(active);
        broadcast(envelope);
    });
}

FileMonitorServer::~FileMonitorServer() = default;

bool FileMonitorServer::listen(const QHostAddress& address, quint16 port)
{
    if (!m_tcpServer.listen(address, port))
        return false;
    m_monitor.start();
    return true;
}

void FileMonitorServer::onNewConnection()
{
    while (QTcpSocket* socket = m_tcpServer.nextPendingConnection()) {
        auto session = std::make_unique<ClientSession>(m_nextSessionId++, socket, m_directory);

        connect(session.get(), &ClientSession::monitoringChangeRequested, this,
                &FileMonitorServer::onMonitoringChangeRequested);
        // Queued: the session must not be destroyed while one of its own
        // signal handlers is still on the call stack.
        connect(session.get(), &ClientSession::closed, this, &FileMonitorServer::onSessionClosed,
                Qt::QueuedConnection);

        session->sendHello(m_monitor.isActive());
        qInfo().noquote() << "Client connected:" << session->peerName()
                          << "- clients:" << m_sessions.size() + 1;
        m_sessions.push_back(std::move(session));
    }
}

void FileMonitorServer::onSessionClosed(quint64 sessionId)
{
    const auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                                 [sessionId](const std::unique_ptr<ClientSession>& session) {
                                     return session->id() == sessionId;
                                 });
    if (it == m_sessions.end())
        return;

    qInfo().noquote() << "Client disconnected:" << (*it)->peerName()
                      << "- clients:" << m_sessions.size() - 1;
    m_sessions.erase(it);
}

void FileMonitorServer::onMonitoringChangeRequested(quint64 sessionId, quint64 requestId,
                                                    bool active)
{
    const bool changed = m_monitor.isActive() != active;
    if (active)
        m_monitor.start();
    else
        m_monitor.stop();

    if (ClientSession* session = findSession(sessionId)) {
        const QString state = active ? QStringLiteral("running") : QStringLiteral("stopped");
        session->sendCommandResult(requestId, true,
                                   changed ? QStringLiteral("Monitoring is now %1").arg(state)
                                           : QStringLiteral("Monitoring was already %1").arg(state));
    }
}

void FileMonitorServer::broadcastReadyEvents()
{
    for (pb::FileEvent& event : m_collector.takeReady()) {
        qInfo().noquote() << QStringLiteral("File %1: %2")
                                 .arg(QLatin1String(eventTypeName(event.type())),
                                      QString::fromStdString(event.file_name()));
        pb::Envelope envelope;
        *envelope.mutable_file_event() = std::move(event);
        broadcast(envelope);
    }
}

void FileMonitorServer::broadcast(const pb::Envelope& envelope)
{
    // Encoded once, the same bytes go to every client.
    const QByteArray frame = encodeFrame(envelope);
    for (const std::unique_ptr<ClientSession>& session : m_sessions)
        session->sendFrame(frame);
}

ClientSession* FileMonitorServer::findSession(quint64 sessionId) const
{
    const auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                                 [sessionId](const std::unique_ptr<ClientSession>& session) {
                                     return session->id() == sessionId;
                                 });
    return it == m_sessions.end() ? nullptr : it->get();
}

} // namespace fm
