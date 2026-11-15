#include "client/EventTableModel.h"

namespace fm {

constexpr int EventTableModel::kMaxRows;

EventTableModel::EventTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int EventTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_records.size());
}

int EventTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant EventTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return QVariant();

    const EventRecord& record = recordAt(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case TimeColumn:
            return record.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        case NameColumn:
            return record.fileName;
        case TypeColumn:
            return eventTypeName(record.type);
        case SizeColumn:
            return record.type == pb::FILE_DELETED ? QStringLiteral("-")
                                                   : formatFileSize(record.size);
        default:
            return QVariant();
        }
    }

    if (role == Qt::ToolTipRole) {
        QString tip = record.filePath;
        if (!record.sha256.isEmpty())
            tip += QStringLiteral("\nSHA-256: ") + QString::fromLatin1(record.sha256.toHex());
        return tip;
    }

    if (role == Qt::TextAlignmentRole && index.column() == SizeColumn)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    return QVariant();
}

QVariant EventTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case TimeColumn:
        return QStringLiteral("Time");
    case NameColumn:
        return QStringLiteral("File name");
    case TypeColumn:
        return QStringLiteral("Event");
    case SizeColumn:
        return QStringLiteral("Size");
    default:
        return QVariant();
    }
}

void EventTableModel::append(const EventRecord& record)
{
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_records.push_back(record);
    endInsertRows();

    const int excess = rowCount() - kMaxRows;
    if (excess > 0) {
        beginRemoveRows(QModelIndex(), 0, excess - 1);
        m_records.erase(m_records.begin(), m_records.begin() + excess);
        endRemoveRows();
    }
}

void EventTableModel::setRecords(std::vector<EventRecord> records)
{
    beginResetModel();
    if (records.size() > static_cast<size_t>(kMaxRows))
        records.erase(records.begin(), records.end() - kMaxRows);
    m_records = std::move(records);
    endResetModel();
}

void EventTableModel::clear()
{
    beginResetModel();
    m_records.clear();
    endResetModel();
}

} // namespace fm
