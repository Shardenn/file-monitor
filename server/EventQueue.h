#pragma once

#include <QMutex>
#include <QObject>
#include <QQueue>

#include <vector>

#include "common/Protocol.h"

namespace fm {

// Thread-safe hand-over of finished file events from the worker thread to
// the thread that owns the network sockets.
class EventQueue : public QObject
{
    Q_OBJECT

public:
    explicit EventQueue(QObject* parent = nullptr);

    // Called from the worker thread.
    void push(pb::FileEvent event);

    // Called from the consumer thread. Removes and returns all queued events
    // in the order they were pushed.
    std::vector<pb::FileEvent> drain();

signals:
    // Emitted from the worker thread when the queue goes from empty to
    // non-empty; a consumer in another thread receives it as a queued call.
    void eventsAvailable();

private:
    QMutex m_mutex;
    QQueue<pb::FileEvent> m_events;
};

} // namespace fm
