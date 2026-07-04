#include "traceline/fusion.hpp"
#include <chrono>

namespace traceline {

namespace {

// Process noise: small for position/velocity (the kinematics are
// exact given constant acceleration), larger for acceleration itself
// (modeled as a random walk -- real acceleration does change).
// These are starting values; tuning against real/simulated flight
// data is future work (see docs/design.md roadmap, M2 follow-up).
constexpr double kProcessNoisePos = 1e-4;
constexpr double kProcessNoiseVel = 1e-3;
constexpr double kProcessNoiseAccel = 1e-1;

// Builds the state-transition matrix F for the constant-acceleration
// model over time step dt, for the 3-axis block-diagonal layout
// [px,py,pz, vx,vy,vz, ax,ay,az].
Mat<9, 9> state_transition(double dt) noexcept {
    Mat<9, 9> f = mat_identity<9>();
    const double half_dt2 = 0.5 * dt * dt;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const std::size_t p = axis;      // position index
        const std::size_t v = axis + 3;  // velocity index
        const std::size_t a = axis + 6;  // acceleration index
        f(p, v) = dt;
        f(p, a) = half_dt2;
        f(v, a) = dt;
    }
    return f;
}

Mat<9, 9> process_noise() noexcept {
    Mat<9, 9> q{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        q(axis, axis) = kProcessNoisePos;
        q(axis + 3, axis + 3) = kProcessNoiseVel;
        q(axis + 6, axis + 6) = kProcessNoiseAccel;
    }
    return q;
}

// Measurement matrix H: the IMU accelerometer directly observes the
// acceleration block of the state (indices 6,7,8).
Mat<3, 9> measurement_matrix() noexcept {
    Mat<3, 9> h{};
    h(0, 6) = 1.0;
    h(1, 7) = 1.0;
    h(2, 8) = 1.0;
    return h;
}

}  // namespace

Result<void> FusionEngine::predict(Timestamp now) noexcept {
    if (!initialized_) {
        estimate_.timestamp = now;
        initialized_ = true;
        sync_estimate();
        return {};
    }

    const auto dt_duration = now - estimate_.timestamp;
    const double dt = std::chrono::duration<double>(dt_duration).count();
    if (dt < 0.0) {
        return std::unexpected(Error::StaleTimestamp);
    }

    const Mat<9, 9> f = state_transition(dt);
    const Mat<9, 9> q = process_noise();

    x_ = mat_mul(f, x_);
    P_ = mat_add(mat_mul(f, mat_mul(P_, mat_transpose(f))), q);
    estimate_.timestamp = now;
    sync_estimate();
    return {};
}

Result<void> FusionEngine::update(const SensorSample<6>& sample) noexcept {
    if (!initialized_) {
        return std::unexpected(Error::NotInitialized);
    }

    const Mat<3, 9> h = measurement_matrix();
    const Mat<9, 3> h_t = mat_transpose(h);

    // Measurement vector: accelerometer xyz (first 3 of the 6-dim IMU sample).
    Mat<3, 1> z{};
    z(0, 0) = sample.values[0];
    z(1, 0) = sample.values[1];
    z(2, 0) = sample.values[2];

    Mat<3, 3> r{};
    r(0, 0) = sample.variance;
    r(1, 1) = sample.variance;
    r(2, 2) = sample.variance;

    const Mat<3, 1> hx = mat_mul(h, x_);
    const Mat<3, 1> y = mat_sub(z, hx);  // innovation

    const Mat<3, 3> s = mat_add(mat_mul(h, mat_mul(P_, h_t)), r);
    const auto s_inv_result = mat3_inverse(s);
    if (!s_inv_result.has_value()) {
        return std::unexpected(s_inv_result.error());
    }
    const Mat<3, 3> s_inv = *s_inv_result;

    const Mat<9, 3> k = mat_mul(P_, mat_mul(h_t, s_inv));  // Kalman gain

    x_ = mat_add(x_, mat_mul(k, y));

    const Mat<9, 9> i9 = mat_identity<9>();
    P_ = mat_mul(mat_sub(i9, mat_mul(k, h)), P_);

    estimate_.timestamp = sample.timestamp;
    sync_estimate();
    return {};
}

void FusionEngine::sync_estimate() noexcept {
    for (std::size_t i = 0; i < 9; ++i) {
        estimate_.state[i] = x_(i, 0);
    }
    for (std::size_t i = 0; i < 81; ++i) {
        estimate_.covariance[i] = P_.data[i];
    }
}

}  // namespace traceline
