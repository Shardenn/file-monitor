#include "client/ReconnectPolicy.h"

#include <algorithm>

namespace fm {

constexpr int ReconnectPolicy::kBaseDelayMs;
constexpr int ReconnectPolicy::kMaxDelayMs;
constexpr double ReconnectPolicy::kJitterFraction;

ReconnectPolicy::ReconnectPolicy()
    : m_random(std::random_device{}())
{
}

int ReconnectPolicy::baseDelayMs() const
{
    // 2^5 * 1 s already exceeds the cap, so the shift cannot overflow.
    const int shift = std::min(m_attempt, 5);
    return std::min(kBaseDelayMs << shift, kMaxDelayMs);
}

int ReconnectPolicy::nextDelayMs()
{
    const int base = baseDelayMs();
    ++m_attempt;

    std::uniform_int_distribution<int> jitter(0, static_cast<int>(base * kJitterFraction));
    return base + jitter(m_random);
}

void ReconnectPolicy::reset()
{
    m_attempt = 0;
}

} // namespace fm
