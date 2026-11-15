#pragma once

#include <QString>

#include <vector>

#include "client/EventRecord.h"

namespace fm {

// Keeps the event history between runs as a JSON file.
class EventHistoryStore
{
public:
    explicit EventHistoryStore(const QString& filePath);

    // The default location inside the user's application data folder.
    static QString defaultFilePath();

    QString filePath() const { return m_filePath; }

    // Returns an empty list if the file is missing or damaged.
    std::vector<EventRecord> load() const;

    // Replaces the file atomically. Returns false on failure.
    bool save(const std::vector<EventRecord>& records) const;

private:
    QString m_filePath;
};

} // namespace fm
