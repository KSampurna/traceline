#include "traceline/telemetry_bus.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <numeric>

namespace traceline {

// @verifies REQ-004
TEST(TelemetryBusTest, PushThenPopRoundTrips) {
    TelemetryBus<int, 8> bus;
    ASSERT_TRUE(bus.push(42).has_value());
    auto result = bus.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(TelemetryBusTest, PopFromEmptyReturnsError) {
    TelemetryBus<int, 8> bus;
    auto result = bus.pop();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::BufferEmpty);
}

TEST(TelemetryBusTest, PushToFullReturnsError) {
    TelemetryBus<int, 2> bus;  // capacity 2 -> 1 usable slot (ring buffer convention)
    ASSERT_TRUE(bus.push(1).has_value());
    auto result = bus.push(2);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::BufferFull);
}

// @verifies REQ-004 -- this is the actual concurrency correctness
// check, run under the tsan-telemetry-bus CI job. A single producer
// thread pushes a known sequence; a single consumer thread pops and
// records what it received. We check two things a broken
// acquire/release pairing could violate: (1) every item the consumer
// reads has a value the producer actually wrote (no torn/garbage
// reads), and (2) the values the consumer receives are a
// strictly-increasing subsequence of what was pushed (no reordering,
// no duplication).
TEST(TelemetryBusTest, ConcurrentPushPopUnderStress) {
    constexpr int kItemCount = 200000;
    TelemetryBus<int, 1024> bus;
    std::atomic<bool> producer_done{false};
    std::vector<int> consumed;
    consumed.reserve(kItemCount);

    std::thread producer([&] {
        for (int i = 0; i < kItemCount; ++i) {
            while (!bus.push(i).has_value()) {
                std::this_thread::yield();  // buffer full, back off
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        while (true) {
            auto result = bus.pop();
            if (result.has_value()) {
                consumed.push_back(*result);
            } else if (producer_done.load(std::memory_order_acquire) &&
                       consumed.size() >= static_cast<size_t>(kItemCount)) {
                break;
            } else if (!result.has_value()) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(kItemCount));
    // Strictly increasing, contiguous 0..N-1 -- proves no reordering,
    // no duplication, and no garbage/torn reads slipped through.
    for (int i = 0; i < kItemCount; ++i) {
        ASSERT_EQ(consumed[static_cast<size_t>(i)], i) << "mismatch at index " << i;
    }
}

}  // namespace traceline
