#include "server/EventQueue.h"

#include <QMutexLocker>

namespace fm {

EventQueue::EventQueue(QObject* parent)
    : QObject(parent)
{
}

void EventQueue::push(pb::FileEvent event)
{
    bool wasEmpty = false;
    {
        const QMutexLocker lock(&m_mutex);
        wasEmpty = m_events.isEmpty();
        m_events.enqueue(std::move(event));
    }
    if (wasEmpty)
        emit eventsAvailable();
}

std::vector<pb::FileEvent> EventQueue::drain()
{
    QQueue<pb::FileEvent> taken;
    {
        const QMutexLocker lock(&m_mutex);
        taken.swap(m_events);
    }

    std::vector<pb::FileEvent> events;
    events.reserve(static_cast<size_t>(taken.size()));
    while (!taken.isEmpty())
        events.push_back(taken.dequeue());
    return events;
}

} // namespace fm
