#include "logflow/model/LogRecord.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace logflow {
namespace {

LogRecord sample() {
    return LogRecord::Builder()
        .timestamp(Timestamp(std::chrono::seconds(1'000'000'000)))
        .clientIp("192.0.2.1")
        .method("POST")
        .path("/login")
        .status(302)
        .bytes(128)
        .userAgent("curl/8.7.1")
        .raw("raw line")
        .build();
}

TEST(LogRecordTest, BuilderSetsEveryField) {
    const LogRecord r = sample();
    EXPECT_EQ(r.timestamp(), Timestamp(std::chrono::seconds(1'000'000'000)));
    EXPECT_EQ(r.clientIp(), "192.0.2.1");
    EXPECT_EQ(r.method(), "POST");
    EXPECT_EQ(r.path(), "/login");
    EXPECT_EQ(r.status(), 302);
    EXPECT_EQ(r.bytes(), 128);
    EXPECT_EQ(r.userAgent(), "curl/8.7.1");
    EXPECT_EQ(r.raw(), "raw line");
    EXPECT_TRUE(r.attributes().empty());
}

TEST(LogRecordTest, WithAttributeReturnsNewRecordAndLeavesOriginalUnchanged) {
    const LogRecord original = sample();
    const LogRecord enriched = original.withAttribute("country", "EG");

    EXPECT_EQ(enriched.attribute("country"), "EG");
    EXPECT_FALSE(original.attribute("country"));
    EXPECT_EQ(enriched.path(), original.path());
}

TEST(LogRecordTest, BuilderCanSetAttributes) {
    const LogRecord r = LogRecord::Builder().attribute("referer", "https://example.com/").build();
    EXPECT_EQ(r.attribute("referer"), "https://example.com/");
    EXPECT_EQ(r.attributes().size(), 1u);
}

TEST(LogRecordTest, IsARecord) {
    const LogRecord r = sample();
    const Record& asRecord = r;
    EXPECT_EQ(asRecord.raw(), "raw line");
}

}  // namespace
}  // namespace logflow
