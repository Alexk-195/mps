#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"

namespace {

struct MyMsg : mps::message {
    int val = 0;
};

struct OtherMsg : mps::message {};

} // namespace

TEST(Waiter, WaitReceivesMatchingMessage) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto w = std::make_shared<mps::waiter<MyMsg>>();
    p->add_worker(w);

    auto sent = std::make_shared<MyMsg>();
    sent->val = 99;
    p->push_back(sent);

    auto got = w->wait(1000);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->val, 99);
}

TEST(Waiter, WaitIgnoresOtherTypes) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto w = std::make_shared<mps::waiter<MyMsg>>();
    p->add_worker(w);

    p->push_back(std::make_shared<OtherMsg>());
    auto got = w->wait(100);
    EXPECT_EQ(got, nullptr);
}

TEST(Waiter, LockingExceptionWhenCallerPrioTooLow) {
    // Calling thread has default prio 0; pool prio is 100 — check() must throw
    auto p = make_started_pool(); // pool_options::priority defaults to 100
    auto w = std::make_shared<mps::waiter<MyMsg>>();
    p->add_worker(w);

    EXPECT_THROW(w->wait(100), mps::locking_exception);
}

TEST(Waiter, ResetAllowsNextMessage) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();
    auto w = std::make_shared<mps::waiter<MyMsg>>();
    p->add_worker(w);

    auto msg1 = std::make_shared<MyMsg>(); msg1->val = 1;
    p->push_back(msg1);
    auto got1 = w->wait(1000);
    ASSERT_NE(got1, nullptr);
    EXPECT_EQ(got1->val, 1);

    w->reset();

    auto msg2 = std::make_shared<MyMsg>(); msg2->val = 2;
    p->push_back(msg2);
    auto got2 = w->wait(1000);
    ASSERT_NE(got2, nullptr);
    EXPECT_EQ(got2->val, 2);
}

TEST(Waiter, MessageWaiterMatchesByIdentity) {
    ScopedThreadPrio high(200);
    auto p = make_started_pool();

    auto target = std::make_shared<const MyMsg>();
    auto other  = std::make_shared<MyMsg>();

    auto mw = std::make_shared<mps::messagewaiter<MyMsg>>(target);
    p->add_worker(mw);

    // Push the wrong message first, then the target
    p->push_back(other);
    p->push_back(target);

    auto got = mw->wait(1000);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got.get(), target.get());
}
