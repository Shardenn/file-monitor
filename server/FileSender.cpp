#include "server/FileSender.h"

#include <algorithm>

namespace fm {

FileSender::FileSender(MessageConnection* connection, QObject* parent)
    : QObject(parent)
    , m_connection(connection)
{
    connect(m_connection, &MessageConnection::bytesWritten, this, &FileSender::pump);
}

void FileSender::enqueue(quint64 requestId, const QString& filePath, const QString& fileName)
{
    m_pending.push_back({requestId, filePath, fileName});
    pump();
}

void FileSender::pump()
{
    if (!m_connection->isConnected()) {
        m_pending.clear();
        m_file.reset();
        return;
    }

    while (m_connection->bytesToWrite() < kSendHighWaterMark) {
        if (!m_file && !openNext())
            return;
        sendNextChunk();
    }
}

// Opens the next pending file that can be opened. Returns false when the
// queue is empty.
bool FileSender::openNext()
{
    while (!m_pending.empty()) {
        m_current = m_pending.front();
        m_pending.pop_front();

        auto file = std::make_unique<QFile>(m_current.filePath);
        if (!file->open(QIODevice::ReadOnly)) {
            emit failed(m_current.requestId,
                        QStringLiteral("Cannot open %1: %2")
                            .arg(m_current.fileName, file->errorString()));
            continue;
        }

        m_file = std::move(file);
        m_totalSize = m_file->size();
        m_offset = 0;
        m_chunkSize = m_totalSize <= kSingleChunkLimit ? m_totalSize : kFileChunkSize;
        return true;
    }
    return false;
}

void FileSender::sendNextChunk()
{
    const qint64 wanted = std::min(m_chunkSize, m_totalSize - m_offset);
    const QByteArray data = wanted > 0 ? m_file->read(wanted) : QByteArray();
    if (data.size() != wanted) {
        failCurrent(QStringLiteral("%1 changed or became unreadable during the transfer")
                        .arg(m_current.fileName));
        return;
    }

    pb::Envelope envelope;
    pb::FileChunk* chunk = envelope.mutable_file_chunk();
    chunk->set_request_id(m_current.requestId);
    chunk->set_file_name(m_current.fileName.toStdString());
    chunk->set_total_size(static_cast<quint64>(m_totalSize));
    chunk->set_offset(static_cast<quint64>(m_offset));
    chunk->set_data(data.constData(), static_cast<size_t>(data.size()));

    m_offset += wanted;
    const bool isLast = m_offset == m_totalSize;
    chunk->set_is_last(isLast);

    m_connection->send(envelope);
    if (isLast)
        m_file.reset();
}

void FileSender::failCurrent(const QString& reason)
{
    m_file.reset();
    emit failed(m_current.requestId, reason);
}

} // namespace fm
