#pragma once

#include "traceline/types.hpp"
#include <atomic>
#include <array>
#include <cstddef>

namespace traceline {

// @req REQ-004: single-producer/single-consumer lock-free ring buffer.
// This is the boundary between the fusion thread (producer) and the
// flight recorder / downstream consumers (consumer thread). No locks,
// no allocation after construction.
//
// Correctness of the acquire/release pairing below is verified by a
// real multithreaded stress test under ThreadSanitizer -- see
// tests/test_telemetry_bus.cpp (ConcurrentPushPopUnderStress) and the
// dedicated tsan-telemetry-bus CI job. Not asserted by comment alone.
template <typename T, std::size_t Capacity>
class TelemetryBus {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    Result<void> push(const T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & (Capacity - 1);
        if (next == tail_.load(std::memory_order_acquire)) {
            return std::unexpected(Error::BufferFull);
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return {};
    }

    Result<T> pop() noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::unexpected(Error::BufferEmpty);
        }
        T item = buffer_[tail];
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    }

private:
    std::array<T, Capacity> buffer_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace traceline
