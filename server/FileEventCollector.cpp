#include "server/FileEventCollector.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent>

namespace fm {

namespace {

constexpr qint64 kHashBlockSize = 64 * 1024;

pb::EventType toEventType(ChangeKind kind)
{
    switch (kind) {
    case ChangeKind::Created:
        return pb::FILE_CREATED;
    case ChangeKind::Modified:
        return pb::FILE_MODIFIED;
    case ChangeKind::Deleted:
        return pb::FILE_DELETED;
    }
    return pb::EVENT_TYPE_UNSPECIFIED;
}

// Returns an empty array if the file cannot be read (for example it was
// deleted or is locked by another process).
QByteArray sha256OfFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray block(static_cast<int>(kHashBlockSize), Qt::Uninitialized);
    for (;;) {
        const qint64 read = file.read(block.data(), block.size());
        if (read < 0)
            return QByteArray();
        if (read == 0)
            break;
        hash.addData(block.constData(), static_cast<int>(read));
    }
    return hash.result();
}

} // namespace

pb::FileEvent buildFileEvent(const QString& directory, const FileChange& change)
{
    const QString path = QDir(directory).absoluteFilePath(change.fileName);

    pb::FileEvent event;
    event.set_type(toEventType(change.kind));
    event.set_file_name(change.fileName.toStdString());
    event.set_file_path(QDir::toNativeSeparators(path).toStdString());

    if (change.kind == ChangeKind::Deleted) {
        event.set_mtime_ms(QDateTime::currentMSecsSinceEpoch());
        return event;
    }

    const QFileInfo info(path);
    event.set_size(static_cast<quint64>(info.size()));
    event.set_mtime_ms(info.lastModified().toMSecsSinceEpoch());

    const QByteArray checksum = sha256OfFile(path);
    event.set_sha256(checksum.constData(), static_cast<size_t>(checksum.size()));
    return event;
}

FileEventCollector::FileEventCollector(const QString& directory, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
{
    // One worker keeps the events in submission order.
    m_pool.setMaxThreadCount(1);
    // Emitted from the worker thread, delivered to our thread as a queued call.
    connect(&m_queue, &EventQueue::eventsAvailable, this, &FileEventCollector::eventsReady);
}

FileEventCollector::~FileEventCollector()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void FileEventCollector::submit(const std::vector<FileChange>& changes)
{
    EventQueue* queue = &m_queue;
    const QString directory = m_directory;
    for (const FileChange& change : changes) {
        QtConcurrent::run(&m_pool, [queue, directory, change] {
            queue->push(buildFileEvent(directory, change));
        });
    }
}

std::vector<pb::FileEvent> FileEventCollector::takeReady()
{
    return m_queue.drain();
}

} // namespace fm
