#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"

// Regression tests for the code-review findings fixed in this change set.

namespace {

struct ProbeMsg : mps::message {};

} // namespace

// Finding 1: a pool that is created but never started must be released when the
// caller drops its shared_ptr. The old permanent self-owning own_ref cycle kept
// it alive forever; the weak_ptr below would never expire.
TEST(ReviewFixes, UnstartedPoolIsFreedWhenDropped) {
    std::weak_ptr<mps::pool> wp;
    {
        auto p = mps::pool::create();
        wp = p;
    }
    EXPECT_TRUE(wp.expired());
}

// Finding 2: waiter::wait() on a waiter that was never added to a pool used to
// dereference a null owner pool and segfault. It must throw instead.
TEST(ReviewFixes, WaiterWithoutOwnerThrowsInsteadOfCrashing) {
    auto w = std::make_shared<mps::waiter<ProbeMsg>>();
    EXPECT_THROW(w->wait(0), mps::exception);
}

// Finding 3: join() must be idempotent. A second join() after the thread has
// already been joined used to call std::thread::join() on a non-joinable thread.
TEST(ReviewFixes, JoinIsIdempotent) {
    auto p = mps::pool::create();
    p->start();
    p->stop();
    p->join();
    EXPECT_NO_THROW(p->join());
    EXPECT_NO_THROW(p->join());
}

// Finding 4: destroying a started+stopped pool without join() must not call
// std::terminate(). keep_alive holds the pool alive until the thread loop exits,
// and the destructor detaches the finished-but-unjoined thread. Reaching the end
// of the test without aborting is the assertion.
TEST(ReviewFixes, DestroyStoppedButNotJoinedDoesNotTerminate) {
    {
        auto p = mps::pool::create();
        p->start();
        p->stop();
        // Intentionally no join(): rely on the destructor to clean up safely.
    }
    SUCCEED();
}

// Finding 5: distributor::create(0, ...) used to be accepted and later divided by
// zero / indexed out of bounds in next_pool(). It must reject n == 0.
TEST(ReviewFixes, DistributorZeroPoolsThrows) {
    EXPECT_THROW(mps::distributor::create(0, mps::pool_options{}, "empty"), mps::exception);
}

// Finding 6: flush() from a thread with insufficient locking priority throws, but
// must still remove its temporary waiter so the pool stays usable afterwards.
TEST(ReviewFixes, FlushThrowsFromLowPrioButPoolStaysUsable) {
    auto p = make_started_pool(); // pool priority defaults to 100

    // Caller priority defaults to 0 (<= 100): wait() inside flush() throws.
    EXPECT_THROW(p->flush(1000), mps::locking_exception);

    // With sufficient priority flush() succeeds, proving the earlier throw did
    // not leave a stale waiter wedged in the pool.
    ScopedThreadPrio high(200);
    EXPECT_TRUE(p->flush(1000));
}

// Finding 7: a pool that cannot set its OS scheduling priority (e.g. T_HIGHER_PRIO
// without privileges) must not terminate the process; the failure is reported and
// the thread continues with default scheduling.
TEST(ReviewFixes, HigherPrioWithoutPrivilegesDoesNotTerminate) {
    ScopedThreadPrio high(200);
    mps::pool_options opts;
    opts.type = mps::pool_options::T_HIGHER_PRIO;
    auto p = make_started_pool(opts);
    // If priority setting had escaped the thread function, the process would have
    // aborted before we ever got here. A successful flush confirms the thread is
    // alive and processing.
    EXPECT_TRUE(p->flush(2000));
}

// Finding 10: pool::remove_worker() must return 0 (per the i_worker_pool contract)
// when the worker belongs to a different pool, instead of queueing a no-op and
// returning a misleading queue size.
TEST(ReviewFixes, RemoveFromWrongPoolReturnsZero) {
    auto p1 = make_started_pool();
    auto p2 = make_started_pool();
    auto w = std::make_shared<CountingWorker>();
    p1->add_worker(w);

    EXPECT_EQ(p2->remove_worker(w), 0u);
}

// Finding 11: timer must use a monotonic clock so wall-clock adjustments cannot
// make elapsed time jump or run backwards.
TEST(ReviewFixes, TimerUsesSteadyClock) {
    EXPECT_TRUE(mps::timer::clock_type::is_steady);
}
