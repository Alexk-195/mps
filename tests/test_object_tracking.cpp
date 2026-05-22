#include <gtest/gtest.h>
#include <mps.h>
#include <sstream>

// These tests only make sense when compiled with -DMPS_TRACK_OBJECTS.
// The CMakeLists.txt builds them in a separate binary (mps_tests_tracking).

TEST(ObjectTracking, DestroyedPoolIsForgotten) {
    {
        auto p = mps::pool::create();
        (void)p;
    }
    // After scope exit the pool is destroyed; dump_all_instances must not crash.
    std::ostringstream oss;
    mps::base::dump_all_instances(oss);
    SUCCEED();
}

TEST(ObjectTracking, LeakedPoolIsReported) {
    // Keep the shared_ptr alive for the duration of the process via static storage.
    static auto leaked = mps::pool::create();

    std::ostringstream oss;
    mps::base::dump_all_instances(oss);
    // The tracking machinery must report at least one instance.
    EXPECT_FALSE(oss.str().empty());
}
