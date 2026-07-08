#include "traceline/fusion.hpp"
#include "traceline/sensor.hpp"
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

namespace {

// Drives an engine/sensor pair for one step the way a real caller would:
// read the sensor, and only call update() if the read succeeded. A failed
// read is not itself an error for the fusion loop -- REQ-003 is about the
// loop surviving it, not about update() ever seeing a bad sample.
void step(FusionEngine& engine, SimulatedImu& sensor, Timestamp t) {
    ASSERT_TRUE(engine.predict(t).has_value());
    auto sample = sensor.read();
    if (sample.has_value()) {
        ASSERT_TRUE(engine.update(*sample).has_value());
    }
}

}  // namespace

// @verifies REQ-003: a dropped sensor read must not crash or corrupt the
// fusion loop -- predict() should keep succeeding and the estimate should
// stay finite even while every read is failing.
TEST(FusionEngineTest, FaultInjectionTest_DropoutDoesNotCrashLoop) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    SimulatedImu sensor({.force_dropout = true});

    auto t = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t).has_value());
    for (int i = 0; i < 50; ++i) {
        t += 5ms;
        step(engine, sensor, t);
    }
    EXPECT_FALSE(std::isnan(engine.estimate().state[8]));
    EXPECT_FALSE(std::isnan(engine.estimate().covariance[(8 * 9) + 8]));
}

// @verifies REQ-003: same guarantee for an out-of-range reading -- the
// sensor reports failure rather than handing the engine a bad sample.
TEST(FusionEngineTest, FaultInjectionTest_OutOfRangeDoesNotCrashLoop) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    SimulatedImu sensor({.force_out_of_range = true});

    auto t = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t).has_value());
    for (int i = 0; i < 50; ++i) {
        t += 5ms;
        step(engine, sensor, t);
    }
    EXPECT_FALSE(std::isnan(engine.estimate().state[8]));
}

// @verifies REQ-003: same guarantee for a stale-timestamp reading.
TEST(FusionEngineTest, FaultInjectionTest_StaleTimestampDoesNotCrashLoop) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    SimulatedImu sensor({.force_stale_timestamp = true});

    auto t = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t).has_value());
    for (int i = 0; i < 50; ++i) {
        t += 5ms;
        step(engine, sensor, t);
    }
    EXPECT_FALSE(std::isnan(engine.estimate().state[8]));
}

// @verifies REQ-003: the loop must not just survive faults, it must
// recover -- once a transient fault clears, the estimate should resume
// converging toward the true value rather than staying stuck on stale
// state accumulated during the outage.
TEST(FusionEngineTest, FaultInjectionTest_RecoversAfterTransientFault) {
    using namespace std::chrono_literals;
    FusionEngine engine;
    SimulatedImu sensor;
    constexpr double kTrueAz = 9.81;

    auto t = std::chrono::steady_clock::now();
    ASSERT_TRUE(engine.predict(t).has_value());

    // 30 steps of dropout (simulated outage).
    sensor.set_faults({.force_dropout = true});
    for (int i = 0; i < 30; ++i) {
        t += 5ms;
        step(engine, sensor, t);
    }

    // Sensor recovers; run enough clean steps to re-converge.
    sensor.set_faults({});
    for (int i = 0; i < 300; ++i) {
        t += 5ms;
        step(engine, sensor, t);
    }

    EXPECT_NEAR(engine.estimate().state[8], kTrueAz, 0.05);
}

}  // namespace traceline
