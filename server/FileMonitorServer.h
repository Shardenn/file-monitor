#pragma once

#include <QObject>
#include <QTcpServer>

#include <memory>
#include <vector>

#include "server/ClientSession.h"
#include "server/DirectoryMonitor.h"
#include "server/FileEventCollector.h"

namespace fm {

// Accepts clients, broadcasts file events to all of them and executes
// their start/stop commands.
class FileMonitorServer : public QObject
{
    Q_OBJECT

public:
    explicit FileMonitorServer(const QString& directory, QObject* parent = nullptr);
    ~FileMonitorServer() override;

    // Starts listening and monitoring. Port 0 picks a free port.
    bool listen(const QHostAddress& address, quint16 port);
    quint16 port() const { return m_tcpServer.serverPort(); }
    QString errorString() const { return m_tcpServer.errorString(); }

    int clientCount() const { return static_cast<int>(m_sessions.size()); }
    bool isMonitoring() const { return m_monitor.isActive(); }

private:
    void onNewConnection();
    void onSessionClosed(quint64 sessionId);
    void onMonitoringChangeRequested(quint64 sessionId, quint64 requestId, bool active);
    void broadcastReadyEvents();
    void broadcast(const pb::Envelope& envelope);
    ClientSession* findSession(quint64 sessionId) const;

    QString m_directory;
    QTcpServer m_tcpServer;
    DirectoryMonitor m_monitor;
    FileEventCollector m_collector;
    std::vector<std::unique_ptr<ClientSession>> m_sessions;
    quint64 m_nextSessionId = 1;
};

} // namespace fm
