#include <gtest/gtest.h>
#include <mps.h>
#include <thread>

TEST(TsQueue, PushPopFifo) {
    mps::ts_queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    int v{};
    EXPECT_TRUE(q.pop(v, 0).second); EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.pop(v, 0).second); EXPECT_EQ(v, 2);
    EXPECT_TRUE(q.pop(v, 0).second); EXPECT_EQ(v, 3);
}

TEST(TsQueue, PopTimeoutReturnsEmpty) {
    mps::ts_queue<int> q;
    int v{};
    auto result = q.pop(v, 10);
    EXPECT_EQ(result.first, 0u);
    EXPECT_FALSE(result.second);
}

TEST(TsQueue, PushToLimitRespectsLimit) {
    mps::ts_queue<int> q;
    bool pushed{};
    q.push_to_limit(1, 2, pushed); EXPECT_TRUE(pushed);
    q.push_to_limit(2, 2, pushed); EXPECT_TRUE(pushed);
    q.push_to_limit(3, 2, pushed); EXPECT_FALSE(pushed);
}

TEST(TsQueue, PushWithClearDropsExisting) {
    mps::ts_queue<int> q;
    q.push(10);
    q.push(20);
    q.push(99, /*clear=*/true);

    int v{};
    EXPECT_TRUE(q.pop(v, 0).second); EXPECT_EQ(v, 99);
    EXPECT_FALSE(q.pop(v, 0).second);
}

TEST(TsQueue, BlockingPopWakesOnPush) {
    mps::ts_queue<int> q;
    int received = -1;
    bool got = false;

    std::thread consumer([&]{
        int v{};
        auto r = q.pop(v, -1); // infinite wait
        received = v;
        got = r.second;
    });

    mps::sleep_ms(20);
    q.push(42);
    consumer.join();

    EXPECT_TRUE(got);
    EXPECT_EQ(received, 42);
}

TEST(TsQueue, PopSizeDecrementsCorrectly) {
    mps::ts_queue<int> q;
    q.push(1);
    q.push(2);

    int v{};
    auto r1 = q.pop(v, 0);
    EXPECT_EQ(r1.first, 1u);

    auto r2 = q.pop(v, 0);
    EXPECT_EQ(r2.first, 0u);
}
