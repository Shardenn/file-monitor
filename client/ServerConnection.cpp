#include "client/ServerConnection.h"

namespace fm {

constexpr int ServerConnection::kConnectTimeoutMs;

ServerConnection::ServerConnection(QObject* parent)
    : QObject(parent)
    , m_connection(new QTcpSocket)
{
    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(kConnectTimeoutMs);
    m_reconnectTimer.setSingleShot(true);

    connect(&m_connectTimer, &QTimer::timeout, this, &ServerConnection::onConnectTimeout);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ServerConnection::startAttempt);
    connect(m_connection.socket(), &QTcpSocket::stateChanged, this,
            &ServerConnection::onSocketStateChanged);
    connect(&m_connection, &MessageConnection::envelopeReceived, this,
            &ServerConnection::onEnvelope);
}

ServerConnection::~ServerConnection()
{
    // The socket is destroyed after our members. Closing it emits
    // stateChanged(), which must not reach a half-destroyed object.
    m_connection.socket()->disconnect(this);
    m_connection.socket()->abort();
}

void ServerConnection::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
    if (!enabled && m_state == State::WaitingToReconnect) {
        m_reconnectTimer.stop();
        m_wanted = false;
        setState(State::Disconnected, QStringLiteral("Reconnect cancelled"));
    }
}

void ServerConnection::connectToServer(const QString& host, quint16 port)
{
    disconnectFromServer();

    m_host = host;
    m_port = port;
    m_wanted = true;
    m_policy.reset();
    startAttempt();
}

void ServerConnection::disconnectFromServer()
{
    m_wanted = false;
    m_connectTimer.stop();
    m_reconnectTimer.stop();
    // abort() is synchronous; onSocketStateChanged() sees m_wanted == false.
    m_connection.socket()->abort();
    setState(State::Disconnected);
}

quint64 ServerConnection::sendStartMonitoring()
{
    pb::ClientCommand command;
    command.mutable_start_monitoring();
    return sendCommand(command);
}

quint64 ServerConnection::sendStopMonitoring()
{
    pb::ClientCommand command;
    command.mutable_stop_monitoring();
    return sendCommand(command);
}

quint64 ServerConnection::requestFile(const QString& fileName)
{
    pb::ClientCommand command;
    command.mutable_file_request()->set_file_name(fileName.toStdString());
    return sendCommand(command);
}

void ServerConnection::startAttempt()
{
    if (!m_wanted)
        return;
    m_timedOut = false;
    setState(State::Connecting, QStringLiteral("%1:%2").arg(m_host).arg(m_port));
    m_connectTimer.start();
    // Asynchronous: the result arrives through stateChanged().
    m_connection.socket()->connectToHost(m_host, m_port);
}

void ServerConnection::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    if (socketState == QAbstractSocket::ConnectedState) {
        m_connectTimer.stop();
        m_policy.reset();
        setState(State::Connected, QStringLiteral("%1:%2").arg(m_host).arg(m_port));
        return;
    }

    if (socketState != QAbstractSocket::UnconnectedState)
        return;

    m_connectTimer.stop();
    if (!m_wanted)
        return; // closed on our side, already reported

    QString reason;
    if (m_timedOut)
        reason = QStringLiteral("Connection timed out");
    else if (m_state == State::Connected)
        reason = QStringLiteral("Connection lost: %1").arg(m_connection.socket()->errorString());
    else
        reason = QStringLiteral("Cannot connect: %1").arg(m_connection.socket()->errorString());
    onLinkDown(reason);
}

void ServerConnection::onConnectTimeout()
{
    m_timedOut = true;
    m_connection.socket()->abort();
}

void ServerConnection::onLinkDown(const QString& reason)
{
    if (!m_autoReconnect) {
        m_wanted = false;
        setState(State::Disconnected, reason);
        return;
    }

    const int delayMs = m_policy.nextDelayMs();
    m_reconnectTimer.start(delayMs);
    setState(State::WaitingToReconnect,
             QStringLiteral("%1. Retrying in %2 s").arg(reason).arg(delayMs / 1000.0, 0, 'f', 1));
}

void ServerConnection::onEnvelope(const pb::Envelope& envelope)
{
    switch (envelope.body_case()) {
    case pb::Envelope::kHello:
        emit helloReceived(envelope.hello());
        break;
    case pb::Envelope::kFileEvent:
        // A type this client does not know is dropped, not shown as garbage.
        if (envelope.file_event().type() != pb::EVENT_TYPE_UNSPECIFIED
            && pb::EventType_IsValid(envelope.file_event().type()))
            emit fileEventReceived(envelope.file_event());
        break;
    case pb::Envelope::kCommandResult:
        emit commandResultReceived(envelope.command_result());
        break;
    case pb::Envelope::kMonitoringState:
        emit monitoringStateReceived(envelope.monitoring_state().active());
        break;
    case pb::Envelope::kFileChunk:
        emit fileChunkReceived(envelope.file_chunk());
        break;
    case pb::Envelope::kCommand:
    case pb::Envelope::BODY_NOT_SET:
        // Not something a server sends, or a message from a newer protocol
        // version. Ignored.
        break;
    }
}

void ServerConnection::setState(State state, const QString& detail)
{
    if (m_state == state && detail.isEmpty())
        return;
    m_state = state;
    emit stateChanged(state, detail);
}

quint64 ServerConnection::sendCommand(pb::ClientCommand& command)
{
    if (!isConnected())
        return 0;

    const quint64 requestId = m_nextRequestId++;
    command.set_request_id(requestId);

    pb::Envelope envelope;
    *envelope.mutable_command() = command;
    return m_connection.send(envelope) ? requestId : 0;
}

} // namespace fm
