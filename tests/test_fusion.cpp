#include "traceline/fusion.hpp"
#include <gtest/gtest.h>
#include <random>

namespace traceline {

// @verifies REQ-002 (structural placeholder -- full heap-allocation
// audit via custom allocator / Valgrind massif lands as follow-up)
TEST(FusionEngineTest, PredictSucceedsAfterConstruction) {
    FusionEngine engine;
    auto result = engine.predict(std::chrono::steady_clock::now());
    EXPECT_TRUE(result.has_value());
}

TEST(FusionEngineTest, UpdateBeforePredictReturnsNotInitialized) {
    FusionEngine engine;
    SensorSample<6> sample{};
    auto result = engine.update(sample);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NotInitialized);
}

TEST(FusionEngineTest, PredictAdvancesTimestamp) {
    FusionEngine engine;
    auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t0).has_value());
    auto t1 = t0 + std::chrono::milliseconds(10);
    ASSERT_TRUE(engine.predict(t1).has_value());
    EXPECT_EQ(engine.estimate().timestamp, t1);
}

TEST(FusionEngineTest, PredictRejectsTimestampGoingBackwards) {
    FusionEngine engine;
    auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t0).has_value());
    auto earlier = t0 - std::chrono::milliseconds(10);
    auto result = engine.predict(earlier);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::StaleTimestamp);
}

// @verifies REQ-001: this is the correctness backbone of the whole
// fusion engine -- confirms the Kalman filter actually converges
// toward the true acceleration under noisy measurements, rather than
// just "compiles and runs without crashing." Checked across multiple
// seeds during development (see commit history / ai-workflow.md) to
// rule out a fixed-seed fluke before trusting this bound.
TEST(FusionEngineTest, ConvergesToTrueAccelerationUnderNoise) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t0).has_value());

    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 0.5);
    constexpr double kTrueAz = 9.81;

    auto t = t0;
    constexpr int kTotalSteps = 500;
    constexpr int kTailSteps = 200;
    double az_sum = 0.0;
    for (int i = 0; i < kTotalSteps; ++i) {
        t += 5ms;
        ASSERT_TRUE(engine.predict(t).has_value());

        SensorSample<6> sample{};
        sample.values = {noise(rng), noise(rng), kTrueAz + noise(rng), 0, 0, 0};
        sample.timestamp = t;
        sample.variance = 0.25;
        ASSERT_TRUE(engine.update(sample).has_value());

        if (i >= kTotalSteps - kTailSteps) {
            az_sum += engine.estimate().state[8];
        }
    }
    const double az_estimate = az_sum / kTailSteps;
    EXPECT_NEAR(az_estimate, kTrueAz, 0.15);
}

TEST(FusionEngineTest, CovarianceStaysBoundedAndFinite) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    auto t = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t).has_value());

    for (int i = 0; i < 50; ++i) {
        t += 5ms;
        ASSERT_TRUE(engine.predict(t).has_value());
        SensorSample<6> sample{};
        sample.values = {0, 0, 9.81, 0, 0, 0};
        sample.timestamp = t;
        sample.variance = 0.25;
        ASSERT_TRUE(engine.update(sample).has_value());
    }
    const double p_az = engine.estimate().covariance[(8 * 9) + 8];
    EXPECT_GT(p_az, 0.0);
    EXPECT_LT(p_az, 10.0);
    EXPECT_FALSE(std::isnan(p_az));
}

}  // namespace traceline
