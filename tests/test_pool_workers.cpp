#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"

namespace {

struct SimpleMsg : mps::message {};

class ThrowingWorker : public mps::worker {
    std::atomic<int> call_count_{0};
public:
    ThrowingWorker() { node_name("ThrowingWorker"); }
    void process(std::shared_ptr<const mps::message>) override {
        call_count_.fetch_add(1, std::memory_order_relaxed);
        throw std::runtime_error("intentional error");
    }
    int call_count() const { return call_count_.load(); }
};

} // namespace

TEST(PoolWorkers, AddNullWorkerThrows) {
    auto p = make_started_pool();
    EXPECT_THROW(p->add_worker(nullptr), mps::exception);
}

TEST(PoolWorkers, AddSameWorkerTwiceThrows) {
    auto p1 = make_started_pool();
    auto p2 = make_started_pool();
    auto w = std::make_shared<CountingWorker>();
    p1->add_worker(w);
    EXPECT_THROW(p2->add_worker(w), mps::exception);
}

TEST(PoolWorkers, RemoveFromWrongPoolNoOps) {
    auto p1 = make_started_pool();
    auto p2 = make_started_pool();
    auto w = std::make_shared<CountingWorker>();
    p1->add_worker(w);
    // p2 does not own w: enqueues internally and no-ops, does not throw
    EXPECT_NO_THROW(p2->remove_worker(w));
    // Worker must still be active in p1
    p1->push_back(std::make_shared<SimpleMsg>());
    EXPECT_TRUE(w->await(1, 1000));
}

TEST(PoolWorkers, WorkerExceptionTriggersRemoval) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto thrower = std::make_shared<ThrowingWorker>();
    auto counter = std::make_shared<CountingWorker>();
    p->add_worker(thrower);
    p->add_worker(counter);

    // First message: thrower throws and gets queued for removal; counter increments
    p->push_back(std::make_shared<SimpleMsg>());
    ASSERT_TRUE(counter->await(1, 2000));

    // Flush ensures removal of thrower is processed before we push again
    ASSERT_TRUE(p->flush(2000));

    // Second message: only counter should see it
    p->push_back(std::make_shared<SimpleMsg>());
    ASSERT_TRUE(counter->await(2, 2000));

    EXPECT_EQ(thrower->call_count(), 1);
}
