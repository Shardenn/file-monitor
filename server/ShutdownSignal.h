#pragma once

#include <QObject>
#include <QTimer>

namespace fm {

// Emits requested() once after the process receives SIGINT or SIGTERM
// (Ctrl+C in the console), so the server can shut down cleanly.
//
// The signal handler only sets a flag, which is the only thing that is safe
// to do inside it on every platform; a timer picks the flag up.
class ShutdownSignal : public QObject
{
    Q_OBJECT

public:
    explicit ShutdownSignal(QObject* parent = nullptr);

signals:
    void requested();

private:
    QTimer m_poll;
};

} // namespace fm
