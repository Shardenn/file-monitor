#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

#include "server/SnapshotDiff.h"

namespace fm {

// Watches the files directly inside one directory and reports which of them
// were created, modified or deleted.
//
// QFileSystemWatcher only says "something changed", so every notification
// triggers a (debounced) rescan that is compared with the previous snapshot.
class DirectoryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryMonitor(const QString& directory, QObject* parent = nullptr);

    QString directory() const { return m_directory; }
    bool isActive() const { return m_active; }

    // Changes that happen while the monitor is stopped are not reported.
    void start();
    void stop();

signals:
    void changesDetected(const std::vector<fm::FileChange>& changes);
    void activeChanged(bool active);

private:
    void rescan();
    void watchFiles(const DirectorySnapshot& snapshot);

    QString m_directory;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    DirectorySnapshot m_snapshot;
    bool m_active = false;
};

} // namespace fm
