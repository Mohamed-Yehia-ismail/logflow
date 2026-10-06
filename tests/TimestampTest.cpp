#include "logflow/util/Timestamp.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace logflow {
namespace {

std::string roundTrip(const std::string& clf) {
    const auto t = parseClfTimestamp(clf);
    return t ? formatIso8601(*t) : "invalid";
}

TEST(TimestampTest, ParsesUtcTimestamp) {
    EXPECT_EQ(roundTrip("01/Sep/2026:08:01:22 +0000"), "2026-09-01T08:01:22Z");
}

TEST(TimestampTest, AppliesPositiveAndNegativeOffsets) {
    EXPECT_EQ(roundTrip("10/Oct/2000:13:55:36 -0700"), "2000-10-10T20:55:36Z");
    EXPECT_EQ(roundTrip("01/Jan/2026:01:30:00 +0200"), "2025-12-31T23:30:00Z");
}

TEST(TimestampTest, UnixEpochIsZero) {
    const auto t = parseClfTimestamp("01/Jan/1970:00:00:00 +0000");
    ASSERT_TRUE(t);
    EXPECT_EQ(t->time_since_epoch().count(), 0);
}

TEST(TimestampTest, HandlesLeapYears) {
    EXPECT_EQ(roundTrip("29/Feb/2024:12:00:00 +0000"), "2024-02-29T12:00:00Z");
    EXPECT_EQ(roundTrip("29/Feb/2000:12:00:00 +0000"), "2000-02-29T12:00:00Z");
    EXPECT_EQ(roundTrip("29/Feb/2026:12:00:00 +0000"), "invalid");
    EXPECT_EQ(roundTrip("29/Feb/1900:12:00:00 +0000"), "invalid");
}

TEST(TimestampTest, RejectsInvalidText) {
    EXPECT_FALSE(parseClfTimestamp(""));
    EXPECT_FALSE(parseClfTimestamp("01/Sep/2026:08:01:22"));
    EXPECT_FALSE(parseClfTimestamp("01-Sep-2026:08:01:22 +0000"));
    EXPECT_FALSE(parseClfTimestamp("01/sep/2026:08:01:22 +0000"));
    EXPECT_FALSE(parseClfTimestamp("00/Sep/2026:08:01:22 +0000"));
    EXPECT_FALSE(parseClfTimestamp("01/Sep/2026:08:60:22 +0000"));
    EXPECT_FALSE(parseClfTimestamp("01/Sep/2026:08:01:60 +0000"));
    EXPECT_FALSE(parseClfTimestamp("01/Sep/2026:08:01:22 *0000"));
    EXPECT_FALSE(parseClfTimestamp("01/Sep/2026:08:01:22 +0075"));
    EXPECT_FALSE(parseClfTimestamp("0a/Sep/2026:08:01:22 +0000"));
}

TEST(TimestampTest, FormatsTimesBeforeTheEpoch) {
    EXPECT_EQ(formatIso8601(Timestamp(std::chrono::seconds(-1))), "1969-12-31T23:59:59Z");
}

}  // namespace
}  // namespace logflow
