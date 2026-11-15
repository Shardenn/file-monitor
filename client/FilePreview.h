#pragma once

#include <QString>

namespace fm {

struct FilePreview
{
    bool isText = false;
    QString text;           // set only for text files
    bool truncated = false; // the file is longer than the preview
    qint64 fileSize = 0;
};

// At most this much of a text file is shown.
constexpr qint64 kPreviewLimitBytes = 1024 * 1024;

// Reads the beginning of a file and decides whether it is text (valid
// UTF-8 without NUL bytes). Blocking; meant to run on a worker thread.
FilePreview loadFilePreview(const QString& filePath, qint64 maxBytes = kPreviewLimitBytes);

} // namespace fm
