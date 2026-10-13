#include "logflow/io/LineFormat.hpp"

#include <gtest/gtest.h>

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

}  // namespace
}  // namespace logflow
