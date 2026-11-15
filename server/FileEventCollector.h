#pragma once

#include <QObject>
#include <QThreadPool>

#include <vector>

#include "common/Protocol.h"
#include "server/EventQueue.h"
#include "server/SnapshotDiff.h"

namespace fm {

// Builds the full event for one change: metadata plus the SHA-256 of the
// file content. Reads the file in small blocks, so it is safe for files of
// any size. Blocking; meant to run on a worker thread.
pb::FileEvent buildFileEvent(const QString& directory, const FileChange& change);

// Turns detected changes into complete FileEvents without blocking the
// caller: hashing runs on a single worker thread, so events keep the order
// in which the changes were submitted.
class FileEventCollector : public QObject
{
    Q_OBJECT

public:
    explicit FileEventCollector(const QString& directory, QObject* parent = nullptr);
    ~FileEventCollector() override;

    void submit(const std::vector<FileChange>& changes);

    // Returns the events finished since the last call.
    std::vector<pb::FileEvent> takeReady();

signals:
    void eventsReady();

private:
    QString m_directory;
    // Declared before the pool: the pool's destructor waits for the running
    // task, which still pushes into the queue.
    EventQueue m_queue;
    QThreadPool m_pool;
};

} // namespace fm
