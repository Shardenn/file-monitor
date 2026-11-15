#include <gtest/gtest.h>

#include "client/ReconnectPolicy.h"

using fm::ReconnectPolicy;

TEST(ReconnectPolicy, BaseDelayDoublesUpToTheCap)
{
    ReconnectPolicy policy;
    const int expected[] = {1000, 2000, 4000, 8000, 16000, 30000, 30000, 30000};
    for (const int base : expected) {
        EXPECT_EQ(policy.baseDelayMs(), base);
        policy.nextDelayMs();
    }
}

TEST(ReconnectPolicy, JitterStaysWithinBounds)
{
    for (int run = 0; run < 50; ++run) {
        ReconnectPolicy policy;
        for (int attempt = 0; attempt < 12; ++attempt) {
            const int base = policy.baseDelayMs();
            const int delay = policy.nextDelayMs();
            EXPECT_GE(delay, base);
            EXPECT_LE(delay, base + static_cast<int>(base * ReconnectPolicy::kJitterFraction));
        }
    }
}

TEST(ReconnectPolicy, ResetStartsOver)
{
    ReconnectPolicy policy;
    for (int i = 0; i < 4; ++i)
        policy.nextDelayMs();
    EXPECT_EQ(policy.attempt(), 4);
    EXPECT_EQ(policy.baseDelayMs(), 16000);

    policy.reset();
    EXPECT_EQ(policy.attempt(), 0);
    EXPECT_EQ(policy.baseDelayMs(), 1000);
}

TEST(ReconnectPolicy, ManyAttemptsDoNotOverflow)
{
    ReconnectPolicy policy;
    for (int i = 0; i < 1000; ++i) {
        const int delay = policy.nextDelayMs();
        ASSERT_GT(delay, 0);
        ASSERT_LE(delay, 36000);
    }
}
