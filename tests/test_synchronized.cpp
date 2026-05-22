#include <gtest/gtest.h>
#include <mps_synchronized.h>
#include <thread>
#include <vector>
#include <atomic>

struct Data {
    int x = 0;
    int y = 0;
    std::string label;
    bool operator==(const Data& o) const {
        return x == o.x && y == o.y && label == o.label;
    }
};

TEST(Synchronized, RoundTripCopy) {
    mps::synchronized<Data> s;
    Data src{42, 99, "hello"};
    s.synced_copy_from(src);

    Data dst{};
    s.synced_copy_to(&dst);
    EXPECT_EQ(dst, src);
}

TEST(Synchronized, ConcurrentReadersAndWriter) {
    mps::synchronized<Data> s;
    std::atomic<bool> stop{false};
    std::atomic<int> bad_reads{0};

    // writer thread
    std::thread writer([&]{
        int i = 0;
        while (!stop.load()) {
            Data d{i, i, std::to_string(i)};
            s.synced_copy_from(d);
            ++i;
        }
    });

    // reader threads: check internal consistency (x == y)
    std::vector<std::thread> readers;
    readers.reserve(4);
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&]{
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            while (std::chrono::steady_clock::now() < deadline) {
                Data d{};
                s.synced_copy_to(&d);
                if (d.x != d.y)
                    bad_reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : readers) t.join();
    stop.store(true);
    writer.join();

    EXPECT_EQ(bad_reads.load(), 0);
}
