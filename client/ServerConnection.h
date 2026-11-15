#pragma once

#include <QObject>
#include <QTimer>

#include "client/ReconnectPolicy.h"
#include "common/MessageConnection.h"

namespace fm {

// The client's link to the server: connects asynchronously, reports its
// state, reconnects with exponential backoff, and exchanges messages.
class ServerConnection : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,      // idle; nothing will happen until connectToServer()
        Connecting,        // a connection attempt is in progress
        Connected,
        WaitingToReconnect // the link failed; a retry is scheduled
    };
    Q_ENUM(State)

    static constexpr int kConnectTimeoutMs = 5000;

    explicit ServerConnection(QObject* parent = nullptr);
    ~ServerConnection() override;

    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    // When enabled (the default), a failed or lost connection is retried
    // automatically until disconnectFromServer() is called.
    void setAutoReconnect(bool enabled);
    bool autoReconnect() const { return m_autoReconnect; }

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();

    // Each returns the request id that the server echoes in its reply,
    // or 0 if not connected.
    quint64 sendStartMonitoring();
    quint64 sendStopMonitoring();
    quint64 requestFile(const QString& fileName);

signals:
    // `detail` explains the state to the user (error text, retry delay).
    void stateChanged(fm::ServerConnection::State state, const QString& detail);
    void helloReceived(const filemonitor::proto::ServerHello& hello);
    void fileEventReceived(const filemonitor::proto::FileEvent& event);
    void commandResultReceived(const filemonitor::proto::CommandResult& result);
    void monitoringStateReceived(bool active);
    void fileChunkReceived(const filemonitor::proto::FileChunk& chunk);

private:
    void startAttempt();
    void onSocketStateChanged(QAbstractSocket::SocketState socketState);
    void onConnectTimeout();
    void onEnvelope(const pb::Envelope& envelope);
    void onLinkDown(const QString& reason);
    void setState(State state, const QString& detail = QString());
    quint64 sendCommand(pb::ClientCommand& command);

    MessageConnection m_connection;
    QTimer m_connectTimer;
    QTimer m_reconnectTimer;
    ReconnectPolicy m_policy;

    State m_state = State::Disconnected;
    QString m_host;
    quint16 m_port = 0;
    bool m_autoReconnect = true;
    // True between connectToServer() and disconnectFromServer().
    bool m_wanted = false;
    bool m_timedOut = false;
    quint64 m_nextRequestId = 1;
};

} // namespace fm
