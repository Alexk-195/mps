#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"

TEST(PoolLifecycle, StartStopJoin) {
    auto p = mps::pool::create();
    p->start();
    p->stop();
    p->join();
}

TEST(PoolLifecycle, DoubleStartThrows) {
    auto p = mps::pool::create();
    p->start();
    EXPECT_THROW(p->start(), mps::exception);
    p->stop();
    p->join();
}

TEST(PoolLifecycle, StopIfNoWorkers) {
    mps::pool_options opts;
    opts.stop_if_no_workers = true;

    auto p = mps::pool::create(opts);
    p->start();
    auto w = std::make_shared<CountingWorker>();
    p->add_worker(w);
    p->remove_worker(w);
    // Pool auto-stops when last worker is removed; join must return
    p->join();
}

TEST(PoolLifecycle, NotificationTimeoutDeliversNotification) {
    mps::pool_options opts;
    opts.timeout_wait_for_message = 50;

    auto p = make_started_pool(opts);
    auto w = std::make_shared<RecordingWorker<mps::notification_message>>();
    p->add_worker(w);

    EXPECT_TRUE(w->await(1, 500));
}
