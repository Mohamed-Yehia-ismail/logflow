#include "logflow/io/LineFormat.hpp"

#include <gtest/gtest.h>

#include "logflow/util/Timestamp.hpp"

namespace logflow {
namespace {

TEST(LineFormatTest, ProjectedRecordUsesKeyValuePairs) {
    EXPECT_EQ(formatLine(ProjectedRecord({{"status", "500"}, {"path", "/api"}})), "status=500 path=/api");
}

TEST(LineFormatTest, ValuesWithSpacesQuotesOrEqualsAreQuoted) {
    EXPECT_EQ(formatLine(ProjectedRecord({{"userAgent", "Mozilla/5.0 (X11)"}})), R"x(userAgent="Mozilla/5.0 (X11)")x");
    EXPECT_EQ(formatLine(ProjectedRecord({{"path", "/search?q=a"}})), R"(path="/search?q=a")");
    EXPECT_EQ(formatLine(ProjectedRecord({{"userAgent", R"(Bot "v2")"}})), R"(userAgent="Bot \"v2\"")");
    EXPECT_EQ(formatLine(ProjectedRecord({{"userAgent", ""}})), R"(userAgent="")");
}

TEST(LineFormatTest, FailedRecordEscapesTheRawInput) {
    const FailedRecord failed{"bad \"line\" \xFF\\", "parse", "line is not valid UTF-8",
                              *parseIso8601("2026-10-27T15:00:00Z"), 10};
    EXPECT_EQ(formatLine(failed),
              "failedAt=2026-10-27T15:00:00Z record=10 stage=parse error=\"line is not valid UTF-8\" "
              "raw=\"bad \\\"line\\\" \\xFF\\\\\"");
}

}  // namespace
}  // namespace logflow
