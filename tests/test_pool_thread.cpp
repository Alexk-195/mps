#include <gtest/gtest.h>
#include <mps.h>
#include <atomic>

TEST(PoolThread, RunsAndJoins) {
    std::atomic<int> call_count{0};
    auto p = mps::pool_thread([&]{ call_count.fetch_add(1, std::memory_order_relaxed); }, "test-thread");
    p->join();
    EXPECT_EQ(call_count.load(), 1);
}
