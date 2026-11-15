#include "client/EventRecord.h"

namespace fm {

EventRecord EventRecord::fromProto(const pb::FileEvent& event)
{
    EventRecord record;
    record.time = QDateTime::fromMSecsSinceEpoch(event.mtime_ms());
    record.fileName = QString::fromStdString(event.file_name());
    record.filePath = QString::fromStdString(event.file_path());
    record.type = event.type();
    record.size = event.size();
    record.sha256 = QByteArray(event.sha256().data(), static_cast<int>(event.sha256().size()));
    return record;
}

QString eventTypeName(pb::EventType type)
{
    switch (type) {
    case pb::FILE_CREATED:
        return QStringLiteral("Created");
    case pb::FILE_MODIFIED:
        return QStringLiteral("Modified");
    case pb::FILE_DELETED:
        return QStringLiteral("Deleted");
    default:
        return QStringLiteral("Unknown");
    }
}

QString formatFileSize(quint64 bytes)
{
    static const char* const units[] = {"KB", "MB", "GB", "TB"};
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);

    double value = static_cast<double>(bytes);
    int unit = -1;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 1).arg(QLatin1String(units[unit]));
}

} // namespace fm
