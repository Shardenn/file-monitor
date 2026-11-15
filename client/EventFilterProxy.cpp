#include "client/EventFilterProxy.h"

#include <QRegularExpression>

#include "client/EventTableModel.h"

namespace fm {

namespace {

// Converts a pattern with * and ? into an unanchored regular expression.
QString wildcardToRegex(const QString& pattern)
{
    QString regex;
    for (const QChar ch : pattern) {
        if (ch == QLatin1Char('*'))
            regex += QStringLiteral(".*");
        else if (ch == QLatin1Char('?'))
            regex += QLatin1Char('.');
        else
            regex += QRegularExpression::escape(QString(ch));
    }
    return regex;
}

} // namespace

EventFilterProxy::EventFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void EventFilterProxy::setTypeFilter(pb::EventType type)
{
    if (m_type == type)
        return;
    m_type = type;
    invalidateFilter();
}

void EventFilterProxy::setNameFilter(const QString& pattern)
{
    const QString trimmed = pattern.trimmed();
    m_hasNamePattern = !trimmed.isEmpty();
    if (m_hasNamePattern) {
        const bool hasWildcards =
            trimmed.contains(QLatin1Char('*')) || trimmed.contains(QLatin1Char('?'));
        // With wildcards the pattern must match the whole name.
        const QString regex = hasWildcards
            ? QLatin1Char('^') + wildcardToRegex(trimmed) + QLatin1Char('$')
            : QRegularExpression::escape(trimmed);
        m_namePattern =
            QRegularExpression(regex, QRegularExpression::CaseInsensitiveOption);
    }
    invalidateFilter();
}

bool EventFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    const auto* model = qobject_cast<const EventTableModel*>(sourceModel());
    if (!model)
        return true;

    const EventRecord& record = model->recordAt(sourceRow);
    if (m_type != pb::EVENT_TYPE_UNSPECIFIED && record.type != m_type)
        return false;
    if (m_hasNamePattern && !m_namePattern.match(record.fileName).hasMatch())
        return false;
    return true;
}

} // namespace fm
