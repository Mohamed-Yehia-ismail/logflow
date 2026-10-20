#include <gtest/gtest.h>

#include "logflow/stages/Predicates.hpp"

namespace logflow {
namespace {

using predicates::compare;
using predicates::ComparisonError;

const LogRecord kRecord = LogRecord::Builder()
                              .timestamp(*parseIso8601("2026-09-01T09:30:00Z"))
                              .clientIp("192.0.2.7")
                              .method("POST")
                              .path("/api/v1/orders")
                              .status(404)
                              .bytes(512)
                              .userAgent("curl/8.7.1")
                              .build();

bool matches(const std::string& field, const std::string& op, const std::string& value) {
    return compare(field, op, value)(kRecord);
}

TEST(CompareTest, NumberFields) {
    EXPECT_TRUE(matches("status", ">=", "400"));
    EXPECT_TRUE(matches("status", ">", "403"));
    EXPECT_FALSE(matches("status", "<", "404"));
    EXPECT_TRUE(matches("status", "<=", "404"));
    EXPECT_TRUE(matches("status", "==", "404"));
    EXPECT_FALSE(matches("status", "!=", "404"));
    EXPECT_TRUE(matches("bytes", ">", "0"));
    EXPECT_FALSE(matches("bytes", ">", "512"));
}

TEST(CompareTest, TimestampField) {
    EXPECT_TRUE(matches("timestamp", ">=", "2026-09-01T09:00:00Z"));
    EXPECT_FALSE(matches("timestamp", "<", "2026-09-01T09:30:00Z"));
}

TEST(CompareTest, TextFields) {
    EXPECT_TRUE(matches("path", "startsWith", "/api"));
    EXPECT_TRUE(matches("path", "endsWith", "/orders"));
    EXPECT_FALSE(matches("path", "endsWith", "a-very-long-suffix-that-is-longer-than-the-path"));
    EXPECT_TRUE(matches("userAgent", "contains", "curl"));
    EXPECT_TRUE(matches("method", "==", "POST"));
    EXPECT_TRUE(matches("clientIp", "!=", "192.0.2.8"));
}

TEST(CompareTest, ErrorsSayWhichPartIsWrong) {
    const auto partOf = [](const std::string& field, const std::string& op, const std::string& value) {
        try {
            compare(field, op, value);
        } catch (const ComparisonError& e) {
            return e.part;
        }
        return std::string("none");
    };
    EXPECT_EQ(partOf("country", "==", "EG"), "field");
    EXPECT_EQ(partOf("status", "startsWith", "4"), "op");
    EXPECT_EQ(partOf("path", ">", "/a"), "op");
    EXPECT_EQ(partOf("status", ">=", "4xx"), "value");
    EXPECT_EQ(partOf("bytes", ">", ""), "value");
    EXPECT_EQ(partOf("timestamp", ">", "yesterday"), "value");
}

}  // namespace
}  // namespace logflow
