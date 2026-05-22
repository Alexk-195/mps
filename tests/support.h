#pragma once

#include <mps.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

// Worker that counts how many times process() has been called.
class CountingWorker : public mps::worker {
    std::atomic<int> count_{0};
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
public:
    CountingWorker() { node_name("CountingWorker"); }

    void process(std::shared_ptr<const mps::message>) override {
        count_.fetch_add(1, std::memory_order_relaxed);
        cv_.notify_all();
    }

    int count() const { return count_.load(std::memory_order_relaxed); }

    // Blocks until count reaches n or timeout_ms elapses. Returns true on success.
    bool await(int n, int timeout_ms) const {
        std::unique_lock<std::mutex> lk(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        return cv_.wait_until(lk, deadline, [&]{ return count() >= n; });
    }
};

// Worker that records every received message of type T_m.
template<class T_m>
class RecordingWorker : public mps::worker {
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<const T_m>> received_;
    mutable std::condition_variable cv_;
public:
    RecordingWorker() { node_name("RecordingWorker"); }

    void process(std::shared_ptr<const mps::message> m) override {
        auto typed = std::dynamic_pointer_cast<const T_m>(m);
        if (typed) {
            std::lock_guard<std::mutex> lk(mutex_);
            received_.push_back(typed);
            cv_.notify_all();
        }
    }

    std::vector<std::shared_ptr<const T_m>> received() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return received_;
    }

    bool await(size_t n, int timeout_ms) const {
        std::unique_lock<std::mutex> lk(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        return cv_.wait_until(lk, deadline, [&]{ return received_.size() >= n; });
    }
};

// RAII wrapper that saves and restores the calling thread's mps priority.
class ScopedThreadPrio {
    unsigned int saved_;
public:
    explicit ScopedThreadPrio(unsigned int prio)
        : saved_(mps::get_this_thread_prio())
    {
        mps::set_this_thread_prio(prio);
    }
    ~ScopedThreadPrio() { mps::set_this_thread_prio(saved_); }
};

// Starts a pool and returns it; stops+joins on scope exit via the deleter.
inline std::shared_ptr<mps::pool> make_started_pool(const mps::pool_options& opts = {}) {
    auto p = mps::pool::create(opts);
    p->start();
    return std::shared_ptr<mps::pool>(p.get(), [p](mps::pool*) mutable {
        p->stop();
        p->join();
        p.reset();
    });
}
