#pragma once

#include <QObject>
#include <QTemporaryFile>

#include <memory>

#include "common/Protocol.h"

namespace fm {

// Reassembles one file from FileChunk messages into a temporary file on
// disk, so a large download never sits in memory.
//
// Handles one download at a time. Chunks and results that do not belong to
// the active download are ignored.
class FileDownloader : public QObject
{
    Q_OBJECT

public:
    explicit FileDownloader(QObject* parent = nullptr);

    bool isActive() const { return m_file != nullptr; }
    QString fileName() const { return m_fileName; }

    // `requestId` is the id returned by ServerConnection::requestFile().
    void begin(quint64 requestId, const QString& fileName);
    void handleChunk(const pb::FileChunk& chunk);
    // A failed CommandResult for the active request aborts the download.
    void handleCommandResult(const pb::CommandResult& result);
    // Aborts the active download, if any (e.g. the connection was lost).
    void abort(const QString& reason);

signals:
    void progress(qint64 receivedBytes, qint64 totalBytes);
    // The temporary file is closed and complete; it is deleted when the
    // last reference goes away.
    void finished(const QString& fileName, std::shared_ptr<QTemporaryFile> file);
    void failed(const QString& fileName, const QString& reason);

private:
    std::shared_ptr<QTemporaryFile> m_file;
    QString m_fileName;
    quint64 m_requestId = 0;
    qint64 m_received = 0;
};

} // namespace fm
