#include "replay_format.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <string>

namespace traceline::tools {

namespace {

FlightRecorder::Entry make_entry() {
    FlightRecorder::Entry entry{};
    entry.timestamp_ns = 1234567890;
    entry.state = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    return entry;
}

}  // namespace

TEST(ReplayFormatTest, TextIncludesIndexTimestampAndStateGroups) {
    const std::string line = format_entry_text(make_entry(), 3);
    EXPECT_NE(line.find("[3]"), std::string::npos);
    EXPECT_NE(line.find("1234567890"), std::string::npos);
    EXPECT_NE(line.find("pos=(1, 2, 3)"), std::string::npos);
    EXPECT_NE(line.find("vel=(4, 5, 6)"), std::string::npos);
    EXPECT_NE(line.find("accel=(7, 8, 9)"), std::string::npos);
}

TEST(ReplayFormatTest, CsvHeaderHasElevenColumns) {
    const std::string header = csv_header();
    EXPECT_EQ(std::count(header.begin(), header.end(), ','), 10);
}

TEST(ReplayFormatTest, CsvRowMatchesEntryFieldsInOrder) {
    const std::string row = format_entry_csv(make_entry(), 0);
    EXPECT_EQ(row, "0,1234567890,1,2,3,4,5,6,7,8,9");
}

}  // namespace traceline::tools
