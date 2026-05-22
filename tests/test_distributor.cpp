#include <gtest/gtest.h>
#include <mps.h>
#include "support.h"
#include <vector>

namespace {

struct SimpleMsg : mps::message {};

// RAII wrapper for distributor lifecycle
struct ScopedDistributor {
    std::shared_ptr<mps::distributor> d;
    explicit ScopedDistributor(size_t n, const mps::pool_options& opts = {}, const std::string& name = "dist") {
        d = mps::distributor::create(n, opts, name);
        d->start();
    }
    ~ScopedDistributor() { d->stop(); d->join(); }
    mps::distributor* operator->() { return d.get(); }
};

} // namespace

TEST(Distributor, RoundRobinAcrossPools) {
    // N=2 pools, K=2: add N*K workers (K per pool), push N*K messages (K per pool)
    const size_t N = 2, K = 2;
    ScopedDistributor sd(N);

    std::vector<std::shared_ptr<CountingWorker>> workers;
    for (size_t i = 0; i < N * K; ++i) {
        auto w = std::make_shared<CountingWorker>();
        workers.push_back(w);
        sd->add_worker(w);
    }

    for (size_t i = 0; i < N * K; ++i)
        sd->push_back(std::make_shared<SimpleMsg>());

    for (auto& w : workers)
        EXPECT_TRUE(w->await(static_cast<int>(K), 3000));
}

TEST(Distributor, RemoveUnknownWorkerReturnsZero) {
    ScopedDistributor sd(2);

    // Worker owned by an unrelated pool — not inside the distributor
    auto other = make_started_pool();
    auto w = std::make_shared<CountingWorker>();
    other->add_worker(w);

    EXPECT_EQ(sd->remove_worker(w), 0u);
}
