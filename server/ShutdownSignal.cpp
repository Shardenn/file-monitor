#include "server/ShutdownSignal.h"

#include <csignal>

namespace fm {

namespace {

volatile std::sig_atomic_t g_shutdownRequested = 0;

extern "C" void onSignal(int)
{
    g_shutdownRequested = 1;
}

constexpr int kPollIntervalMs = 200;

} // namespace

ShutdownSignal::ShutdownSignal(QObject* parent)
    : QObject(parent)
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    m_poll.setInterval(kPollIntervalMs);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (!g_shutdownRequested)
            return;
        m_poll.stop();
        emit requested();
    });
    m_poll.start();
}

} // namespace fm
