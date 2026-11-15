#include "server/ClientSession.h"

#include <QHostAddress>
#include <QLoggingCategory>

#include "server/FileRequestValidator.h"

namespace fm {

ClientSession::ClientSession(quint64 id, QTcpSocket* socket, const QString& directory,
                             QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_directory(directory)
    , m_peerName(QStringLiteral("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort()))
    , m_connection(socket)
    , m_fileSender(&m_connection)
{
    connect(&m_connection, &MessageConnection::envelopeReceived, this, &ClientSession::onEnvelope);
    connect(&m_connection, &MessageConnection::disconnected, this, [this] { emit closed(m_id); });
    connect(&m_connection, &MessageConnection::protocolError, this, [this](const QString& error) {
        qWarning().noquote() << "Client" << m_peerName << "violated the protocol:" << error;
    });
    connect(&m_fileSender, &FileSender::failed, this,
            [this](quint64 requestId, const QString& reason) {
                sendCommandResult(requestId, false, reason);
            });
}

void ClientSession::sendHello(bool monitoringActive)
{
    pb::Envelope envelope;
    pb::ServerHello* hello = envelope.mutable_hello();
    hello->set_protocol_version(kProtocolVersion);
    hello->set_watched_directory(m_directory.toStdString());
    hello->set_monitoring_active(monitoringActive);
    m_connection.send(envelope);
}

void ClientSession::sendCommandResult(quint64 requestId, bool success, const QString& message)
{
    pb::Envelope envelope;
    pb::CommandResult* result = envelope.mutable_command_result();
    result->set_request_id(requestId);
    result->set_success(success);
    result->set_message(message.toStdString());
    m_connection.send(envelope);
}

void ClientSession::sendFrame(const QByteArray& frame)
{
    m_connection.sendFrame(frame);
}

void ClientSession::onEnvelope(const pb::Envelope& envelope)
{
    if (envelope.body_case() == pb::Envelope::kCommand) {
        onCommand(envelope.command());
        return;
    }
    // Anything else is either a message only the server may send, or a
    // message type added by a newer protocol version. Ignore it.
    qWarning().noquote() << "Client" << m_peerName << "sent an unexpected message, ignored";
}

void ClientSession::onCommand(const pb::ClientCommand& command)
{
    switch (command.action_case()) {
    case pb::ClientCommand::kStartMonitoring:
        emit monitoringChangeRequested(m_id, command.request_id(), true);
        break;
    case pb::ClientCommand::kStopMonitoring:
        emit monitoringChangeRequested(m_id, command.request_id(), false);
        break;
    case pb::ClientCommand::kFileRequest:
        onFileRequest(command.request_id(), command.file_request());
        break;
    case pb::ClientCommand::ACTION_NOT_SET:
        sendCommandResult(command.request_id(), false, QStringLiteral("Unknown command"));
        break;
    }
}

void ClientSession::onFileRequest(quint64 requestId, const pb::FileRequest& request)
{
    const QString fileName = QString::fromStdString(request.file_name());

    QString error;
    const QString path = resolveRequestedFile(m_directory, fileName, &error);
    if (path.isEmpty()) {
        sendCommandResult(requestId, false, error);
        return;
    }

    qInfo().noquote() << "Client" << m_peerName << "requested" << fileName;
    m_fileSender.enqueue(requestId, path, fileName);
}

} // namespace fm
