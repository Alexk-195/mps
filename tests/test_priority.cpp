#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"
#include <atomic>

namespace {

class PriorityCapturingWorker : public mps::worker {
    std::atomic<unsigned int> observed_{0};
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::atomic<bool> done_{false};
public:
    PriorityCapturingWorker() { node_name("PriorityCapturingWorker"); }

    void process(std::shared_ptr<const mps::message>) override {
        observed_.store(mps::get_this_thread_prio(), std::memory_order_relaxed);
        done_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lk(mutex_);
        cv_.notify_all();
    }

    bool await(int timeout_ms) const {
        std::unique_lock<std::mutex> lk(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        return cv_.wait_until(lk, deadline, [&]{ return done_.load(std::memory_order_acquire); });
    }

    unsigned int observed() const { return observed_.load(std::memory_order_relaxed); }
};

} // namespace

TEST(Priority, GetSetRoundTrip) {
    ScopedThreadPrio guard(42);
    EXPECT_EQ(mps::get_this_thread_prio(), 42u);
}

TEST(Priority, PoolPropagatesPriorityToTls) {
    mps::pool_options opts;
    opts.priority = 77;

    auto p = make_started_pool(opts);
    auto w = std::make_shared<PriorityCapturingWorker>();
    p->add_worker(w);

    struct Trigger : mps::message {};
    p->push_back(std::make_shared<Trigger>());

    ASSERT_TRUE(w->await(1000));
    EXPECT_EQ(w->observed(), 77u);
}
