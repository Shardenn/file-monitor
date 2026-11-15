#include "client/EventHistoryStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <iterator>

namespace fm {

namespace {

constexpr int kFormatVersion = 1;

QJsonObject toJson(const EventRecord& record)
{
    QJsonObject object;
    object[QStringLiteral("time_ms")] = static_cast<double>(record.time.toMSecsSinceEpoch());
    object[QStringLiteral("file_name")] = record.fileName;
    object[QStringLiteral("file_path")] = record.filePath;
    object[QStringLiteral("type")] = static_cast<int>(record.type);
    object[QStringLiteral("size")] = static_cast<double>(record.size);
    object[QStringLiteral("sha256")] = QString::fromLatin1(record.sha256.toHex());
    return object;
}

EventRecord fromJson(const QJsonObject& object)
{
    EventRecord record;
    record.time = QDateTime::fromMSecsSinceEpoch(
        static_cast<qint64>(object[QStringLiteral("time_ms")].toDouble()));
    record.fileName = object[QStringLiteral("file_name")].toString();
    record.filePath = object[QStringLiteral("file_path")].toString();

    const int type = object[QStringLiteral("type")].toInt();
    record.type = pb::EventType_IsValid(type) ? static_cast<pb::EventType>(type)
                                              : pb::EVENT_TYPE_UNSPECIFIED;
    record.size = static_cast<quint64>(object[QStringLiteral("size")].toDouble());
    record.sha256 = QByteArray::fromHex(object[QStringLiteral("sha256")].toString().toLatin1());
    return record;
}

} // namespace

EventHistoryStore::EventHistoryStore(const QString& filePath)
    : m_filePath(filePath)
{
}

QString EventHistoryStore::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("event_history.json"));
}

std::vector<EventRecord> EventHistoryStore::load() const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray events = document.object()[QStringLiteral("events")].toArray();

    std::vector<EventRecord> records;
    records.reserve(static_cast<size_t>(events.size()));
    std::transform(events.begin(), events.end(), std::back_inserter(records),
                   [](const QJsonValue& value) { return fromJson(value.toObject()); });
    return records;
}

bool EventHistoryStore::save(const std::vector<EventRecord>& records) const
{
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath()))
        return false;

    QJsonArray events;
    for (const EventRecord& record : records)
        events.append(toJson(record));

    QJsonObject root;
    root[QStringLiteral("version")] = kFormatVersion;
    root[QStringLiteral("events")] = events;

    // QSaveFile writes to a temporary file and renames it, so a crash in the
    // middle never leaves a half-written history.
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace fm
