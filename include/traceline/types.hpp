#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <chrono>

namespace traceline {

// Monotonic timestamp type used throughout the fusion pipeline.
// Deliberately not wall-clock time -- fusion math needs monotonicity,
// not calendar semantics.
using Timestamp = std::chrono::steady_clock::time_point;

// @req REQ-006: all fallible operations in the public API return
// std::expected rather than throwing. This is a hard rule on the
// fusion hot path (see docs/design.md section 4).
enum class Error : std::uint8_t {
    SensorTimeout,
    SensorOutOfRange,
    StaleTimestamp,
    BufferFull,
    BufferEmpty,
    ChecksumMismatch,
    NotInitialized,
    SingularMatrix,
};

template <typename T>
using Result = std::expected<T, Error>;

// Fixed-capacity, no-heap-allocation vector-like buffer used on the
// fusion hot path. A real implementation would flesh this out; this
// is the seam where "no dynamic allocation after init" (REQ-002) is
// enforced structurally rather than by convention.
template <typename T, std::size_t Capacity>
class FixedBuffer {
public:
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

    Result<void> push_back(const T& value) noexcept {
        if (size_ >= Capacity) {
            return std::unexpected(Error::BufferFull);
        }
        data_[size_++] = value;
        return {};
    }

private:
    std::array<T, Capacity> data_{};
    std::size_t size_ = 0;
};

}  // namespace traceline
