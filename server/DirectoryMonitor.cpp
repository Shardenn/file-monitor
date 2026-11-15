#include "server/DirectoryMonitor.h"

#include <QDir>

#include <unordered_set>

namespace fm {

namespace {
// A burst of notifications (e.g. a file being copied) becomes one rescan.
constexpr int kDebounceMs = 100;
} // namespace

DirectoryMonitor::DirectoryMonitor(const QString& directory, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &DirectoryMonitor::rescan);

    const auto scheduleRescan = [this] {
        if (m_active && !m_debounce.isActive())
            m_debounce.start();
    };
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, scheduleRescan);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, scheduleRescan);
}

void DirectoryMonitor::start()
{
    if (m_active)
        return;
    m_active = true;
    m_snapshot = takeSnapshot(m_directory);
    m_watcher.addPath(m_directory);
    watchFiles(m_snapshot);
    emit activeChanged(true);
}

void DirectoryMonitor::stop()
{
    if (!m_active)
        return;
    m_active = false;
    m_debounce.stop();

    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    m_snapshot.clear();
    emit activeChanged(false);
}

void DirectoryMonitor::rescan()
{
    if (!m_active)
        return;

    DirectorySnapshot current = takeSnapshot(m_directory);
    const std::vector<FileChange> changes = diffSnapshots(m_snapshot, current);
    m_snapshot = std::move(current);

    // The watcher drops a path when the file is deleted or replaced (many
    // editors save by renaming), so the watch list is refreshed every time.
    if (!m_watcher.directories().contains(m_directory))
        m_watcher.addPath(m_directory);
    watchFiles(m_snapshot);

    if (!changes.empty())
        emit changesDetected(changes);
}

// The directory watch reports created and deleted files; modifications of
// existing files are only reported for files that are watched themselves.
void DirectoryMonitor::watchFiles(const DirectorySnapshot& snapshot)
{
    const QStringList alreadyWatched = m_watcher.files();
    const std::unordered_set<QString, QStringHasher> watched(alreadyWatched.begin(),
                                                             alreadyWatched.end());
    const QDir dir(m_directory);

    QStringList toAdd;
    for (const auto& entry : snapshot) {
        const QString path = dir.absoluteFilePath(entry.first);
        if (watched.find(path) == watched.end())
            toAdd.append(path);
    }
    if (!toAdd.isEmpty())
        m_watcher.addPaths(toAdd);
}

} // namespace fm
