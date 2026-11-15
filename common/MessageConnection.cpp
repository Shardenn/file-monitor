#include "common/MessageConnection.h"

#include <QPointer>

namespace fm {

MessageConnection::MessageConnection(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
    Q_ASSERT(socket);
    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &MessageConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &MessageConnection::bytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, &MessageConnection::onDisconnected);
}

bool MessageConnection::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

qint64 MessageConnection::bytesToWrite() const
{
    return m_socket->bytesToWrite();
}

bool MessageConnection::send(const pb::Envelope& envelope)
{
    return sendFrame(encodeFrame(envelope));
}

bool MessageConnection::sendFrame(const QByteArray& frame)
{
    if (frame.isEmpty() || !isConnected())
        return false;
    return m_socket->write(frame) == frame.size();
}

void MessageConnection::onReadyRead()
{
    m_decoder.append(m_socket->readAll());

    // A slot connected to envelopeReceived() may close or delete this
    // connection, so it is re-checked after every emitted message.
    const QPointer<MessageConnection> self(this);
    pb::Envelope envelope;
    for (;;) {
        const FrameDecoder::Result result = m_decoder.next(envelope);
        if (result == FrameDecoder::Result::NeedMoreData)
            return;
        if (result == FrameDecoder::Result::Error) {
            const QString error = m_decoder.errorString();
            m_socket->abort();
            if (self)
                emit protocolError(error);
            return;
        }
        emit envelopeReceived(envelope);
        if (!self || !isConnected())
            return;
    }
}

void MessageConnection::onDisconnected()
{
    // The socket object can be reused for the next connection; leftovers of
    // the old stream must not leak into it.
    m_decoder.reset();
    emit disconnected();
}

} // namespace fm
