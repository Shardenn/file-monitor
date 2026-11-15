#pragma once

#include <QRegularExpression>
#include <QSortFilterProxyModel>

#include "common/Protocol.h"

namespace fm {

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
    QRegularExpression m_namePattern;
    bool m_hasNamePattern = false;
};

} // namespace fm
