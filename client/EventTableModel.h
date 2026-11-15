#pragma once

#include <QAbstractTableModel>

#include <vector>

#include "client/EventRecord.h"

namespace fm {

// Table of received file events, oldest first.
class EventTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { TimeColumn, NameColumn, TypeColumn, SizeColumn, ColumnCount };

    // The oldest rows are dropped beyond this many, so a long session does
    // not grow without bound.
    static constexpr int kMaxRows = 10000;

    explicit EventTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void append(const EventRecord& record);
    void setRecords(std::vector<EventRecord> records);
    void clear();

    const std::vector<EventRecord>& records() const { return m_records; }
    const EventRecord& recordAt(int row) const { return m_records[static_cast<size_t>(row)]; }

private:
    std::vector<EventRecord> m_records;
};

} // namespace fm
