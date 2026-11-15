#pragma once

#include <QObject>

#include "common/MessageConnection.h"
#include "server/FileSender.h"

namespace fm {

// Server-side state of one connected client: decodes its commands, answers
// them, and streams requested files.
class ClientSession : public QObject
{
    Q_OBJECT

public:
    // Takes ownership of the socket.
    ClientSession(quint64 id, QTcpSocket* socket, const QString& directory,
                  QObject* parent = nullptr);

    quint64 id() const { return m_id; }
    QString peerName() const { return m_peerName; }

    void sendHello(bool monitoringActive);
    void sendCommandResult(quint64 requestId, bool success, const QString& message);
    // Sends an already encoded frame (used for broadcasts).
    void sendFrame(const QByteArray& frame);

signals:
    // The client asked to start or stop monitoring. The owner decides and
    // answers with sendCommandResult().
    void monitoringChangeRequested(quint64 sessionId, quint64 requestId, bool active);
    // The connection is gone; the session can be destroyed.
    void closed(quint64 sessionId);

private:
    void onEnvelope(const pb::Envelope& envelope);
    void onCommand(const pb::ClientCommand& command);
    void onFileRequest(quint64 requestId, const pb::FileRequest& request);

    quint64 m_id;
    QString m_directory;
    QString m_peerName;
    MessageConnection m_connection;
    FileSender m_fileSender;
};

} // namespace fm
