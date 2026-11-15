#pragma once

#include <QHash>
#include <QString>

#include <unordered_map>
#include <vector>

namespace fm {

// What we remember about a file between two directory scans.
struct FileStamp
{
    qint64 size = 0;
    qint64 mtimeMs = 0;

    bool operator==(const FileStamp& other) const
    {
        return size == other.size && mtimeMs == other.mtimeMs;
    }
    bool operator!=(const FileStamp& other) const { return !(*this == other); }
};

// std::hash<QString> only exists since Qt 5.14; we support 5.12.
struct QStringHasher
{
    size_t operator()(const QString& value) const { return qHash(value); }
};

// File name -> stamp, for the regular files directly inside one directory.
using DirectorySnapshot = std::unordered_map<QString, FileStamp, QStringHasher>;

enum class ChangeKind { Created, Modified, Deleted };

struct FileChange
{
    ChangeKind kind;
    QString fileName;

    bool operator==(const FileChange& other) const
    {
        return kind == other.kind && fileName == other.fileName;
    }
};

// Lists the regular files of a directory (not recursive).
DirectorySnapshot takeSnapshot(const QString& directory);

// Compares two snapshots. The result is sorted by file name so that the
// output is deterministic.
std::vector<FileChange> diffSnapshots(const DirectorySnapshot& before,
                                      const DirectorySnapshot& after);

} // namespace fm
