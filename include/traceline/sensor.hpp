#pragma once

#include "traceline/types.hpp"
#include <array>
#include <concepts>

namespace traceline {

// A single reading from a sensor. Fixed size, trivially copyable --
// this type must never own heap memory, since it crosses the
// producer/consumer boundary on the telemetry bus (REQ-004).
template <std::size_t Dim>
struct SensorSample {
    std::array<double, Dim> values{};
    Timestamp timestamp{};
    double variance = 0.0;  // sensor-reported measurement uncertainty
};

// @req REQ-003: any type modeling SensorSource must expose a way to
// fetch a reading that can fail (dropout, out-of-range, stale data)
// without throwing -- fault injection is a first-class concern, not
// an afterthought bolted onto a happy-path interface.
template <typename S>
concept SensorSource = requires(S s) {
    typename S::SampleType;
    { s.read() } -> std::same_as<Result<typename S::SampleType>>;
};

// Simulated IMU: 6-axis (accel xyz, gyro xyz). Fault injection is
// configured externally so tests can force dropout / out-of-range /
// stale-timestamp conditions deterministically (REQ-003).
class SimulatedImu {
public:
    using SampleType = SensorSample<6>;

    struct FaultConfig {
        bool force_dropout = false;
        bool force_out_of_range = false;
        bool force_stale_timestamp = false;
    };

    SimulatedImu() = default;
    explicit SimulatedImu(FaultConfig faults) : faults_(faults) {}

    Result<SampleType> read() const {
        if (faults_.force_dropout) {
            return std::unexpected(Error::SensorTimeout);
        }
        if (faults_.force_out_of_range) {
            return std::unexpected(Error::SensorOutOfRange);
        }
        if (faults_.force_stale_timestamp) {
            return std::unexpected(Error::StaleTimestamp);
        }
        // Placeholder deterministic reading -- real noise model lands in M1.
        return SampleType{.values = {0, 0, 9.81, 0, 0, 0},
                           .timestamp = std::chrono::steady_clock::now(),
                           .variance = 0.01};
    }

    void set_faults(FaultConfig faults) { faults_ = faults; }

private:
    FaultConfig faults_;
};

static_assert(SensorSource<SimulatedImu>,
              "SimulatedImu must satisfy the SensorSource concept");

}  // namespace traceline
