#pragma once

#include <QFile>
#include <QObject>

#include <deque>
#include <memory>

#include "common/MessageConnection.h"

namespace fm {

// Sends requested files to one client as a sequence of FileChunk messages.
//
// The file is read one chunk at a time, and only while the socket's send
// buffer is below kSendHighWaterMark; the next chunk is read when the socket
// reports that bytes were written. So a file of any size uses a bounded
// amount of memory and never blocks the event loop for long.
//
// Requests are served one after another in the order they arrived.
class FileSender : public QObject
{
    Q_OBJECT

public:
    explicit FileSender(MessageConnection* connection, QObject* parent = nullptr);

    void enqueue(quint64 requestId, const QString& filePath, const QString& fileName);

signals:
    void failed(quint64 requestId, const QString& reason);

private:
    struct Request
    {
        quint64 requestId;
        QString filePath;
        QString fileName;
    };

    void pump();
    bool openNext();
    void sendNextChunk();
    void failCurrent(const QString& reason);

    MessageConnection* m_connection;
    std::deque<Request> m_pending;

    // State of the transfer in progress; m_file is null when idle.
    std::unique_ptr<QFile> m_file;
    Request m_current{};
    qint64 m_totalSize = 0;
    qint64 m_offset = 0;
    qint64 m_chunkSize = 0;
};

} // namespace fm
