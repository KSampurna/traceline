#include "traceline/fusion.hpp"
#include "traceline/sensor.hpp"
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

// @req REQ-001, REQ-003: property-based tests per docs/design.md section 5
// -- fusion convergence and fault-tolerance checked across randomized
// inputs (RapidCheck-generated), not just the fixed-seed/fixed-fault-type
// cases in test_fusion.cpp.

namespace traceline {

namespace {

SimulatedImu::FaultConfig fault_config_for(int code) {
    switch (code) {
        case 1:
            return {.force_dropout = true};
        case 2:
            return {.force_out_of_range = true};
        case 3:
            return {.force_stale_timestamp = true};
        default:
            return {};
    }
}

}  // namespace

// @verifies REQ-001: generalizes test_fusion.cpp's
// ConvergesToTrueAccelerationUnderNoise (fixed seed=42, fixed true
// acceleration) into a property that must hold for any reasonable true
// acceleration, noise variance, and RNG seed -- not just the one
// hand-picked combination.
RC_GTEST_PROP(FusionEngineProperties, ConvergesToTrueAccelerationForAnyReasonableInput, ()) {
    // Generated as scaled integers rather than via a floating-point
    // range generator, to avoid depending on unverified inRange<double>
    // support -- inRange<int> is unambiguously supported.
    const double true_az = static_cast<double>(*rc::gen::inRange(-2000, 2000)) / 100.0;  // -20.00..19.99
    const double variance = static_cast<double>(*rc::gen::inRange(1, 100)) / 100.0;      // 0.01..0.99
    const unsigned seed = static_cast<unsigned>(*rc::gen::inRange(0, 1000000));

    using namespace std::chrono_literals;
    FusionEngine engine;
    auto t = std::chrono::steady_clock::now();
    RC_ASSERT(engine.predict(t).has_value());

    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, std::sqrt(variance));

    constexpr int kTotalSteps = 500;
    constexpr int kTailSteps = 200;
    double az_sum = 0.0;
    for (int i = 0; i < kTotalSteps; ++i) {
        t += 5ms;
        RC_ASSERT(engine.predict(t).has_value());

        SensorSample<6> sample{};
        sample.values = {noise(rng), noise(rng), true_az + noise(rng), 0, 0, 0};
        sample.timestamp = t;
        sample.variance = variance;
        RC_ASSERT(engine.update(sample).has_value());

        if (i >= kTotalSteps - kTailSteps) {
            az_sum += engine.estimate().state[8];
        }
    }
    const double az_estimate = az_sum / kTailSteps;
    // Tolerance scales with injected noise: a fixed absolute bound would
    // either be too loose for near-noiseless input or spuriously fail on
    // noisy input.
    RC_ASSERT(std::abs(az_estimate - true_az) < (0.1 + (3.0 * std::sqrt(variance))));
}

// @verifies REQ-003: for any random interleaving of valid sensor reads and
// fault types (dropout / out-of-range / stale timestamp), the fusion loop
// must never crash, every predict()/update() call that does run must
// return a well-defined Result, and the state estimate and covariance
// must stay finite throughout -- "degrades gracefully" made concrete
// rather than asserted in a comment.
RC_GTEST_PROP(FusionEngineProperties, SurvivesAnyFaultInterleaving, ()) {
    using namespace std::chrono_literals;
    const int step_count = *rc::gen::inRange(1, 200);
    const auto fault_codes = *rc::gen::container<std::vector<int>>(
        static_cast<std::size_t>(step_count), rc::gen::inRange(0, 4));

    FusionEngine engine;
    SimulatedImu sensor;
    auto t = std::chrono::steady_clock::now();
    RC_ASSERT(engine.predict(t).has_value());

    for (int code : fault_codes) {
        sensor.set_faults(fault_config_for(code));
        t += 5ms;
        RC_ASSERT(engine.predict(t).has_value());

        auto sample = sensor.read();
        if (sample.has_value()) {
            RC_ASSERT(engine.update(*sample).has_value());
        }

        const double az = engine.estimate().state[8];
        RC_ASSERT(!std::isnan(az));
        RC_ASSERT(!std::isinf(az));
        const double p_az = engine.estimate().covariance[(8 * 9) + 8];
        RC_ASSERT(!std::isnan(p_az));
        RC_ASSERT(!std::isinf(p_az));
    }
}

}  // namespace traceline
