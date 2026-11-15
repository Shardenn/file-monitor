#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include "common/Protocol.h"

namespace fm {

// One file event as the client keeps it (in the table and in the history).
struct EventRecord
{
    QDateTime time;      // modification time; event time for deleted files
    QString fileName;
    QString filePath;
    pb::EventType type = pb::EVENT_TYPE_UNSPECIFIED;
    quint64 size = 0;
    QByteArray sha256;

    static EventRecord fromProto(const pb::FileEvent& event);
};

// "Created", "Modified", "Deleted" or "Unknown".
QString eventTypeName(pb::EventType type);

// Human-readable size, e.g. "1.5 MB".
QString formatFileSize(quint64 bytes);

} // namespace fm
