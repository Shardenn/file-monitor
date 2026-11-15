#pragma once

#include <QSortFilterProxyModel>

#include "common/Protocol.h"

namespace fm {

// Case-insensitive match of `text` against a pattern where * stands for any
// run of characters (including none) and ? for exactly one character.
bool matchesWildcard(const QString& pattern, const QString& text);

// Shows only the events that match the chosen type and file name.
// The source model must be an EventTableModel.
class EventFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit EventFilterProxy(QObject* parent = nullptr);

    // EVENT_TYPE_UNSPECIFIED shows all types.
    void setTypeFilter(pb::EventType type);
    // Case-insensitive; supports the wildcards * and ?. Without wildcards
    // the text matches anywhere in the file name. Empty shows all names.
    void setNameFilter(const QString& pattern);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    pb::EventType m_type = pb::EVENT_TYPE_UNSPECIFIED;
    QString m_namePattern; // empty: no name filter
};

} // namespace fm
