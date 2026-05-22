#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"

namespace {

struct NumberedMsg : mps::message {
    int n;
    explicit NumberedMsg(int val) : n(val) {}
};

struct SimpleMsg : mps::message {};

class SlowWorker : public mps::worker {
public:
    SlowWorker() { node_name("SlowWorker"); }
    void process(std::shared_ptr<const mps::message>) override {
        mps::sleep_ms(300);
    }
};

} // namespace

TEST(PoolMessages, MessagesArriveInOrder) {
    auto p = make_started_pool();
    auto w = std::make_shared<RecordingWorker<NumberedMsg>>();
    p->add_worker(w);

    for (int i = 0; i < 5; ++i)
        p->push_back(std::make_shared<NumberedMsg>(i));

    ASSERT_TRUE(w->await(5, 2000));
    auto received = w->received();
    ASSERT_EQ(received.size(), 5u);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(received[static_cast<size_t>(i)]->n, i);
}

TEST(PoolMessages, MultipleWorkersAllReceiveMessage) {
    auto p = make_started_pool();
    auto w1 = std::make_shared<RecordingWorker<NumberedMsg>>();
    auto w2 = std::make_shared<RecordingWorker<NumberedMsg>>();
    p->add_worker(w1);
    p->add_worker(w2);

    p->push_back(std::make_shared<NumberedMsg>(42));

    ASSERT_TRUE(w1->await(1, 1000));
    ASSERT_TRUE(w2->await(1, 1000));
    EXPECT_EQ(w1->received()[0]->n, 42);
    EXPECT_EQ(w2->received()[0]->n, 42);
}

TEST(PoolMessages, PushBackToLimitRejectsWhenFull) {
    // Don't start the pool so messages accumulate in the queue
    auto p = mps::pool::create();
    bool pushed{};
    p->push_back_to_limit(std::make_shared<SimpleMsg>(), 2, pushed); EXPECT_TRUE(pushed);
    p->push_back_to_limit(std::make_shared<SimpleMsg>(), 2, pushed); EXPECT_TRUE(pushed);
    p->push_back_to_limit(std::make_shared<SimpleMsg>(), 2, pushed); EXPECT_FALSE(pushed);
    p->start();
    p->stop();
    p->join();
}

TEST(PoolMessages, FlushReturnsTrueWhenDrained) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto w = std::make_shared<CountingWorker>();
    p->add_worker(w);

    p->push_back(std::make_shared<SimpleMsg>());
    EXPECT_TRUE(p->flush(2000));
}

TEST(PoolMessages, FlushReturnsFalseOnTimeout) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto w = std::make_shared<SlowWorker>();
    p->add_worker(w);

    // Worker takes 300ms; flush timeout is 50ms
    p->push_back(std::make_shared<SimpleMsg>());
    EXPECT_FALSE(p->flush(50));
}
