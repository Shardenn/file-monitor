#include "client/EventFilterProxy.h"

#include "client/EventTableModel.h"

namespace fm {

// Classic greedy matching with backtracking to the last *. A plain loop
// instead of QRegularExpression: the patterns are tiny, and the regex JIT
// produces false alarms under valgrind.
bool matchesWildcard(const QString& pattern, const QString& text)
{
    int p = 0;
    int t = 0;
    int starInPattern = -1;
    int starInText = 0;

    while (t < text.size()) {
        if (p < pattern.size() && pattern[p] == QLatin1Char('*')) {
            starInPattern = p++;
            starInText = t;
        } else if (p < pattern.size()
                   && (pattern[p] == QLatin1Char('?')
                       || pattern[p].toCaseFolded() == text[t].toCaseFolded())) {
            ++p;
            ++t;
        } else if (starInPattern >= 0) {
            // Let the last * swallow one more character and retry.
            p = starInPattern + 1;
            t = ++starInText;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == QLatin1Char('*'))
        ++p;
    return p == pattern.size();
}

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
    QString normalized = pattern.trimmed();
    const bool hasWildcards =
        normalized.contains(QLatin1Char('*')) || normalized.contains(QLatin1Char('?'));
    // Plain text matches anywhere in the name; a pattern with wildcards
    // must match the whole name.
    if (!normalized.isEmpty() && !hasWildcards)
        normalized = QLatin1Char('*') + normalized + QLatin1Char('*');

    if (m_namePattern == normalized)
        return;
    m_namePattern = normalized;
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
    if (!m_namePattern.isEmpty() && !matchesWildcard(m_namePattern, record.fileName))
        return false;
    return true;
}

} // namespace fm
