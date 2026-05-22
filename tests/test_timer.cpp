#include <gtest/gtest.h>
#include <mps.h>

TEST(Timer, ElapsedIsMonotonic) {
    mps::timer t;
    mps::sleep_ms(5);
    double first = t.elapsed();
    mps::sleep_ms(5);
    double second = t.elapsed();
    EXPECT_GE(second, first);
}

TEST(Timer, ResetReturnsToZero) {
    mps::timer t;
    mps::sleep_ms(20);
    t.reset();
    EXPECT_LT(t.elapsed(), 10.0);
}

TEST(Timer, ConstructorWithoutResetStaysFrozen) {
    mps::timer t(false);
    EXPECT_DOUBLE_EQ(t.elapsed(), 0.0);
}

TEST(Timer, ElapsedApproximatesActualSleep) {
    mps::timer t;
    mps::sleep_ms(50);
    double e = t.elapsed();
    EXPECT_GE(e, 50.0);
    EXPECT_LT(e, 250.0);
}
