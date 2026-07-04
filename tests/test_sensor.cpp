#include "traceline/sensor.hpp"
#include <gtest/gtest.h>

namespace traceline {

// @verifies REQ-003
TEST(SimulatedImuTest, HappyPathReturnsValue) {
    SimulatedImu imu;
    auto result = imu.read();
    ASSERT_TRUE(result.has_value());
}

// @verifies REQ-003
TEST(SimulatedImuTest, DropoutFaultReturnsTimeoutError) {
    SimulatedImu imu({.force_dropout = true});
    auto result = imu.read();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::SensorTimeout);
}

// @verifies REQ-003
TEST(SimulatedImuTest, OutOfRangeFaultReturnsError) {
    SimulatedImu imu({.force_out_of_range = true});
    auto result = imu.read();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::SensorOutOfRange);
}

}  // namespace traceline
