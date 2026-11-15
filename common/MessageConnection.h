#pragma once

#include <QObject>
#include <QTcpSocket>

#include "common/FrameCodec.h"

namespace fm {

// Turns a QTcpSocket byte stream into a stream of Envelopes and back.
// All I/O is asynchronous and driven by the socket's signals.
class MessageConnection : public QObject
{
    Q_OBJECT

public:
    // Takes ownership of the socket (it becomes a child of this object).
    explicit MessageConnection(QTcpSocket* socket, QObject* parent = nullptr);

    QTcpSocket* socket() const { return m_socket; }
    bool isConnected() const;
    qint64 bytesToWrite() const;

    // Queues the message for sending. Returns false if it could not be
    // queued (not connected, or the message is too large).
    bool send(const pb::Envelope& envelope);
    // Queues an already encoded frame (see encodeFrame()).
    bool sendFrame(const QByteArray& frame);

signals:
    void envelopeReceived(const filemonitor::proto::Envelope& envelope);
    void bytesWritten(qint64 bytes);
    void disconnected();
    // The peer sent data that violates the framing; the socket was aborted.
    void protocolError(const QString& description);

private:
    void onReadyRead();
    void onDisconnected();

    QTcpSocket* m_socket;
    FrameDecoder m_decoder;
};

} // namespace fm
