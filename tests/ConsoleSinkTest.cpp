#include "logflow/io/ConsoleSink.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "logflow/util/Timestamp.hpp"

namespace logflow {
namespace {

LogRecord::Builder base() {
    LogRecord::Builder builder;
    builder.timestamp(*parseClfTimestamp("01/Sep/2026:08:01:22 +0000"))
        .clientIp("192.0.2.38")
        .method("GET")
        .path("/contact")
        .status(200)
        .bytes(18224);
    return builder;
}

TEST(ConsoleSinkTest, PrintsRecordOnOneReadableLine) {
    std::ostringstream out;
    ConsoleSink sink(out);
    sink.consume(base().userAgent("curl/8.7.1").build());
    EXPECT_EQ(out.str(), "2026-09-01T08:01:22Z  192.0.2.38  GET /contact  200  18224 B  \"curl/8.7.1\"\n");
}

TEST(ConsoleSinkTest, OmitsUserAgentWhenThereIsNone) {
    std::ostringstream out;
    ConsoleSink sink(out);
    sink.consume(base().build());
    EXPECT_EQ(out.str(), "2026-09-01T08:01:22Z  192.0.2.38  GET /contact  200  18224 B\n");
}

}  // namespace
}  // namespace logflow
