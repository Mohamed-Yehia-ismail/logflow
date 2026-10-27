#include "logflow/stages/ValidationStage.hpp"

#include <gtest/gtest.h>

#include "logflow/core/StageException.hpp"
#include "support/CollectingEmitter.hpp"

namespace logflow {
namespace {

using test::CollectingEmitter;

LogRecord record(const std::string& ip = "192.0.2.10", int status = 200, long long bytes = 100,
                 const std::string& time = "2026-10-15T10:15:32Z", const std::string& path = "/index.html") {
    return LogRecord::Builder()
        .timestamp(*parseIso8601(time))
        .clientIp(ip)
        .method("GET")
        .path(path)
        .status(status)
        .bytes(bytes)
        .build();
}

// Runs one record through a stage with `rules`; returns the error, or "" if it passed.
std::string check(std::vector<ValidationRule> rules, const LogRecord& r) {
    ValidationStage stage(std::move(rules));
    CollectingEmitter<LogRecord> out;
    try {
        stage.process(r, out);
    } catch (const StageException& e) {
        EXPECT_TRUE(out.items.empty());
        return e.what();
    }
    EXPECT_EQ(out.items.size(), 1u);
    return "";
}

TEST(ValidationStageTest, ValidRecordPassesUnchanged) {
    ValidationStage stage({validation::range("status", 100, 599), validation::ipFormat("ipv4")});
    CollectingEmitter<LogRecord> out;
    stage.process(record(), out);
    ASSERT_EQ(out.items.size(), 1u);
    EXPECT_EQ(out.items[0].clientIp(), "192.0.2.10");
}

TEST(ValidationStageTest, RequiredField) {
    EXPECT_EQ(check({validation::required("path")}, record("192.0.2.10", 200, 1, "2026-10-15T10:15:32Z", "")),
              "validation failed: path is missing");
    EXPECT_EQ(check({validation::required("method")}, record()), "");
    EXPECT_EQ(check({validation::required("userAgent")}, record()), "validation failed: userAgent is missing");
    EXPECT_EQ(check({validation::required("clientIp")}, record()), "");
    EXPECT_THROW(validation::required("status"), std::invalid_argument);
}

TEST(ValidationStageTest, NumericRange) {
    EXPECT_EQ(check({validation::range("bytes", 0, std::nullopt)}, record("192.0.2.10", 200, -45)),
              "validation failed: bytes -45 is outside 0..");
    EXPECT_EQ(check({validation::range("status", 100, 399)}, record("192.0.2.10", 404)),
              "validation failed: status 404 is outside 100..399");
    EXPECT_EQ(check({validation::range("bytes", std::nullopt, 100)}, record()), "");
    EXPECT_THROW(validation::range("path", 0, 1), std::invalid_argument);
}

TEST(ValidationStageTest, TimestampRangeRejects1970And2099) {
    const auto rule = [] {
        return validation::timestampRange(parseIso8601("2000-01-01T00:00:00Z"), parseIso8601("2030-12-31T23:59:59Z"));
    };
    EXPECT_EQ(check({rule()}, record("192.0.2.10", 200, 1, "1970-01-01T00:00:00Z")),
              "validation failed: timestamp 1970-01-01T00:00:00Z is outside 2000-01-01T00:00:00Z..2030-12-31T23:59:59Z");
    EXPECT_NE(check({rule()}, record("192.0.2.10", 200, 1, "2099-12-31T23:59:59Z")), "");
    EXPECT_EQ(check({rule()}, record()), "");
}

TEST(ValidationStageTest, IpFormat) {
    for (const char* ip : {"192.0.2.10", "0.0.0.0", "255.255.255.255"}) {
        EXPECT_EQ(check({validation::ipFormat("ipv4")}, record(ip)), "") << ip;
    }
    for (const char* ip : {"256.0.0.1", "1.2.3", "1.2.3.4.5", "01.2.3.4", "a.b.c.d", "", "1..2.3"}) {
        EXPECT_NE(check({validation::ipFormat("ipv4")}, record(ip)), "") << ip;
    }
    for (const char* ip : {"2001:db8::1", "::1", "::", "2001:0db8:0000:0000:0000:ff00:0042:8329"}) {
        EXPECT_EQ(check({validation::ipFormat("ipv6")}, record(ip)), "") << ip;
    }
    for (const char* ip : {"2001:db8::1::2", "12345::1", "1:2:3:4:5:6:7", "g::1", "192.0.2.10"}) {
        EXPECT_NE(check({validation::ipFormat("ipv6")}, record(ip)), "") << ip;
    }
    EXPECT_EQ(check({validation::ipFormat("ip")}, record("::1")), "");
    EXPECT_EQ(check({validation::ipFormat("ip")}, record("192.0.2.10")), "");
    EXPECT_EQ(check({validation::ipFormat("ip")}, record("not-an-ip")),
              "validation failed: clientIp 'not-an-ip' is not an IP address");
    EXPECT_THROW(validation::ipFormat("mac"), std::invalid_argument);
}

TEST(ValidationStageTest, EveryBrokenRuleIsReported) {
    EXPECT_EQ(check({validation::range("status", 100, 599), validation::range("bytes", 0, std::nullopt),
                     validation::ipFormat("ipv4")},
                    record("x", 200, -1)),
              "validation failed: bytes -1 is outside 0..; clientIp 'x' is not an IPv4 address");
}

TEST(ValidationStageTest, NeedsAtLeastOneRule) {
    EXPECT_THROW(ValidationStage({}), std::invalid_argument);
}

TEST(ValidationStageTest, SplitRange) {
    EXPECT_EQ(validation::splitRange("100..599"), std::make_pair(std::string("100"), std::string("599")));
    EXPECT_EQ(validation::splitRange("0.."), std::make_pair(std::string("0"), std::string("")));
    EXPECT_FALSE(validation::splitRange("100"));
}

}  // namespace
}  // namespace logflow
