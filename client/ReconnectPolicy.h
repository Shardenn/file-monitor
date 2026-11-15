#pragma once

#include <random>

namespace fm {

// Exponential backoff for reconnect attempts: 1 s, 2 s, 4 s ... up to a
// cap, each with a little random jitter so that many clients do not retry
// in lockstep after a server restart.
class ReconnectPolicy
{
public:
    static constexpr int kBaseDelayMs = 1000;
    static constexpr int kMaxDelayMs = 30000;
    // The jitter adds up to this fraction of the delay.
    static constexpr double kJitterFraction = 0.2;

    ReconnectPolicy();

    // Delay before the next attempt; every call doubles the base delay.
    int nextDelayMs();

    // Delay of the next attempt without jitter (does not advance).
    int baseDelayMs() const;

    // Call after a successful connection.
    void reset();

    int attempt() const { return m_attempt; }

private:
    int m_attempt = 0;
    std::mt19937 m_random;
};

} // namespace fm
