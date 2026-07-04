#include "traceline/flight_recorder.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <vector>

namespace traceline {

namespace {

// Unique temp file per test process run, so parallel test runs don't
// collide. Cleaned up in each test via RAII-ish scope guard below.
std::string temp_path(std::string_view name) {
    return std::string("/tmp/traceline_flight_recorder_test_") + std::string(name) + ".bin";
}

StateEstimate make_estimate(double x, double y, double z, std::int64_t ns) {
    StateEstimate e{};
    e.state = {x, y, z, 0, 0, 0, 0, 0, 0};
    e.timestamp = Timestamp(std::chrono::nanoseconds(ns));
    return e;
}

}  // namespace

// @verifies REQ-005
TEST(FlightRecorderTest, RecordThenReplayRoundTrips) {
    const std::string path = temp_path("roundtrip");
    std::remove(path.c_str());

    {
        FlightRecorder recorder(path);
        ASSERT_TRUE(recorder.record(make_estimate(1.0, 2.0, 3.0, 1000)).has_value());
        ASSERT_TRUE(recorder.record(make_estimate(4.0, 5.0, 6.0, 2000)).has_value());
        ASSERT_TRUE(recorder.record(make_estimate(7.0, 8.0, 9.0, 3000)).has_value());
    }

    std::vector<FlightRecorder::Entry> entries;
    FlightRecorder reader(path);
    auto result = reader.replay([&](const FlightRecorder::Entry& e) { entries.push_back(e); });
    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].timestamp_ns, 1000);
    EXPECT_DOUBLE_EQ(entries[0].state[0], 1.0);
    EXPECT_DOUBLE_EQ(entries[0].state[1], 2.0);
    EXPECT_DOUBLE_EQ(entries[0].state[2], 3.0);
    EXPECT_EQ(entries[2].timestamp_ns, 3000);
    EXPECT_DOUBLE_EQ(entries[2].state[2], 9.0);

    std::remove(path.c_str());
}

// @verifies REQ-005 -- this is the actual point of the checksum: a
// single flipped bit anywhere in a record must be detected, not
// silently trusted.
TEST(FlightRecorderTest, CorruptionDetectionTest) {
    const std::string path = temp_path("corruption");
    std::remove(path.c_str());

    {
        FlightRecorder recorder(path);
        ASSERT_TRUE(recorder.record(make_estimate(1.0, 2.0, 3.0, 1000)).has_value());
        ASSERT_TRUE(recorder.record(make_estimate(4.0, 5.0, 6.0, 2000)).has_value());
    }

    // Flip one bit in the middle of the first record's payload.
    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(f.is_open());
        f.seekp(10);  // inside the first record's state bytes
        char byte = 0;
        f.seekg(10);
        f.read(&byte, 1);
        byte = static_cast<char>(byte ^ 0xFF);
        f.seekp(10);
        f.write(&byte, 1);
    }

    std::vector<FlightRecorder::Entry> entries;
    FlightRecorder reader(path);
    auto result = reader.replay([&](const FlightRecorder::Entry& e) { entries.push_back(e); });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ChecksumMismatch);
    // Corruption was in the first record, so zero valid entries should
    // have been delivered to the visitor before replay stopped.
    EXPECT_EQ(entries.size(), 0u);

    std::remove(path.c_str());
}

// @verifies REQ-005 -- a file truncated mid-record (e.g. crash during
// write) must be reported as an error, not silently ignored or
// misread as a shorter valid log.
TEST(FlightRecorderTest, TruncatedRecordIsDetected) {
    const std::string path = temp_path("truncated");
    std::remove(path.c_str());

    {
        FlightRecorder recorder(path);
        ASSERT_TRUE(recorder.record(make_estimate(1.0, 2.0, 3.0, 1000)).has_value());
    }

    // Truncate the file to half a record.
    {
        std::ofstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        f.close();
    }
    std::string data;
    {
        std::ifstream in(path, std::ios::binary);
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(data.data(), static_cast<std::streamsize>(data.size() / 2));
    }

    std::vector<FlightRecorder::Entry> entries;
    FlightRecorder reader(path);
    auto result = reader.replay([&](const FlightRecorder::Entry& e) { entries.push_back(e); });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ChecksumMismatch);
    EXPECT_EQ(entries.size(), 0u);

    std::remove(path.c_str());
}

TEST(FlightRecorderTest, ReplayOnMissingFileReturnsError) {
    const std::string path = temp_path("does_not_exist");
    std::remove(path.c_str());

    FlightRecorder reader(path);
    std::vector<FlightRecorder::Entry> entries;
    auto result = reader.replay([&](const FlightRecorder::Entry& e) { entries.push_back(e); });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NotInitialized);
}

}  // namespace traceline
