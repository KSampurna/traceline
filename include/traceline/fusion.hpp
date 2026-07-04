#pragma once

#include "traceline/linalg.hpp"
#include "traceline/sensor.hpp"
#include "traceline/types.hpp"
#include <array>

namespace traceline {

// Fused state estimate: constant-acceleration model per axis.
// State layout: [px,py,pz, vx,vy,vz, ax,ay,az] -- position, velocity,
// and acceleration for x/y/z. Acceleration is modeled as a random
// walk (see docs/design.md); this is what lets the IMU accelerometer
// act as a direct linear measurement of part of the state, rather
// than needing a nonlinear (extended) filter step. Orientation
// estimation is out of scope for v1 -- see docs/design.md Non-Goals.
struct StateEstimate {
    std::array<double, 9> state{};        // see layout above
    std::array<double, 81> covariance{};  // 9x9, row-major
    Timestamp timestamp{};
};

// @req REQ-001, REQ-002: predict/update run in bounded time (fixed
// 9x9 / 3x3 matrix operations, no loops with data-dependent bounds)
// with no heap allocation after construction -- all working memory
// is fixed-size members.
class FusionEngine {
public:
    FusionEngine() = default;

    // Advances the filter's time estimate using the constant-acceleration
    // process model. First call after construction anchors the filter's
    // initial timestamp and does not advance the state.
    Result<void> predict(Timestamp now) noexcept;

    // Incorporates a new IMU sample as a direct measurement of the
    // acceleration component of the state (indices 6,7,8). Returns
    // Error::NotInitialized if called before the first predict().
    Result<void> update(const SensorSample<6>& sample) noexcept;

    [[nodiscard]] const StateEstimate& estimate() const noexcept { return estimate_; }

private:
    void sync_estimate() noexcept;  // copies internal x_/P_ into estimate_

    Mat<9, 1> x_{};  // state vector
    Mat<9, 9> P_{};  // state covariance
    StateEstimate estimate_{};
    bool initialized_ = false;
};

}  // namespace traceline
