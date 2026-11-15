#include "server/SnapshotDiff.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace fm {

DirectorySnapshot takeSnapshot(const QString& directory)
{
    DirectorySnapshot snapshot;
    const QFileInfoList entries =
        QDir(directory).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    snapshot.reserve(static_cast<size_t>(entries.size()));
    for (const QFileInfo& info : entries) {
        FileStamp stamp;
        stamp.size = info.size();
        stamp.mtimeMs = info.lastModified().toMSecsSinceEpoch();
        snapshot.emplace(info.fileName(), stamp);
    }
    return snapshot;
}

std::vector<FileChange> diffSnapshots(const DirectorySnapshot& before,
                                      const DirectorySnapshot& after)
{
    std::vector<FileChange> changes;

    for (const auto& entry : after) {
        const auto old = before.find(entry.first);
        if (old == before.end())
            changes.push_back({ChangeKind::Created, entry.first});
        else if (old->second != entry.second)
            changes.push_back({ChangeKind::Modified, entry.first});
    }
    for (const auto& entry : before) {
        if (after.find(entry.first) == after.end())
            changes.push_back({ChangeKind::Deleted, entry.first});
    }

    std::sort(changes.begin(), changes.end(), [](const FileChange& a, const FileChange& b) {
        return a.fileName < b.fileName;
    });
    return changes;
}

} // namespace fm
