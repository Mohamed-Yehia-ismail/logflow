#include "logflow/stages/ParserStage.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "logflow/util/Timestamp.hpp"
#include "support/CollectingEmitter.hpp"

namespace logflow {
namespace {

using test::CollectingEmitter;

const std::string kValidLine =
    R"(192.0.2.38 - - [01/Sep/2026:08:01:22 +0000] "GET /contact HTTP/1.1" 200 18224 )"
    R"("https://www.example.com/" "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_5) Safari/605.1.15")";

class ParserStageTest : public ::testing::Test {
protected:
    // Feeds one line to the stage and returns how many records it emitted.
    std::size_t feed(const std::string& line) {
        const std::size_t before = out.items.size();
        stage.process(line, out);
        return out.items.size() - before;
    }

    std::ostringstream report;
    ParserStage stage{report};
    CollectingEmitter<LogRecord> out;
};

// 1. Valid line
TEST_F(ParserStageTest, ValidLineIsParsedIntoAllFields) {
    ASSERT_EQ(feed(kValidLine), 1u);

    const LogRecord& r = out.items[0];
    EXPECT_EQ(formatIso8601(r.timestamp()), "2026-09-01T08:01:22Z");
    EXPECT_EQ(r.clientIp(), "192.0.2.38");
    EXPECT_EQ(r.method(), "GET");
    EXPECT_EQ(r.path(), "/contact");
    EXPECT_EQ(r.status(), 200);
    EXPECT_EQ(r.bytes(), 18224);
    EXPECT_EQ(r.userAgent(), "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_5) Safari/605.1.15");
    EXPECT_EQ(r.attribute("referer"), "https://www.example.com/");
    EXPECT_EQ(r.raw(), kValidLine);
    EXPECT_EQ(stage.parsedCount(), 1u);
    EXPECT_EQ(stage.malformedCount(), 0u);
}

// 2. Missing field
TEST_F(ParserStageTest, LineWithMissingFieldIsSkippedAndCounted) {
    // No byte count after the status code.
    EXPECT_EQ(feed(R"(192.0.2.14 - - [01/Sep/2026:08:41:07 +0000] "GET /index.html HTTP/1.1" 200)"), 0u);
    // No request line at all.
    EXPECT_EQ(feed(R"(192.0.2.14 - - [01/Sep/2026:08:41:07 +0000] 200 512)"), 0u);
    // Request line without a protocol.
    EXPECT_EQ(feed(R"(192.0.2.14 - - [01/Sep/2026:08:41:07 +0000] "GET /index.html" 200 512)"), 0u);
    EXPECT_EQ(stage.malformedCount(), 3u);
}

// 3. Malformed timestamp
TEST_F(ParserStageTest, LineWithMalformedTimestampIsSkippedAndCounted) {
    const std::string bad[] = {
        R"(192.0.2.7 - - [31/Sep/2026:09:12:44 +0000] "GET / HTTP/1.1" 200 5120)",   // no 31st
        R"(192.0.2.7 - - [01/Foo/2026:09:12:44 +0000] "GET / HTTP/1.1" 200 5120)",   // month
        R"(192.0.2.7 - - [01/Sep/2026:25:12:44 +0000] "GET / HTTP/1.1" 200 5120)",   // hour
        R"(192.0.2.7 - - [2026-09-01T09:12:44Z] "GET / HTTP/1.1" 200 5120)",         // format
        R"(192.0.2.7 - - [01/Sep/2026:09:12:44] "GET / HTTP/1.1" 200 5120)",         // no zone
    };
    for (const std::string& line : bad) EXPECT_EQ(feed(line), 0u) << line;
    EXPECT_EQ(stage.malformedCount(), 5u);
}

// 4. Malformed status code
TEST_F(ParserStageTest, LineWithMalformedStatusIsSkippedAndCounted) {
    const std::string bad[] = {
        R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET /cart HTTP/1.1" 2OO 7781)",  // letters
        R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET /cart HTTP/1.1" 999 7781)",  // range
        R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET /cart HTTP/1.1" 20 7781)",   // length
        R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET /cart HTTP/1.1" - 7781)",    // dash
    };
    for (const std::string& line : bad) EXPECT_EQ(feed(line), 0u) << line;
    EXPECT_EQ(stage.malformedCount(), 4u);
}

// 5. Empty line
TEST_F(ParserStageTest, EmptyOrBlankLineIsSkippedAndCounted) {
    EXPECT_EQ(feed(""), 0u);
    EXPECT_EQ(feed("   \t  "), 0u);
    EXPECT_EQ(stage.malformedCount(), 2u);
    EXPECT_EQ(stage.parsedCount(), 0u);
}

// 6. Extra whitespace
TEST_F(ParserStageTest, ExtraWhitespaceBetweenAndAroundFieldsIsAccepted) {
    const std::string line =
        "   192.0.2.38   -  -   [01/Sep/2026:08:01:22 +0000]   \"GET   /contact   HTTP/1.1\"   "
        "200    18224   \t";
    ASSERT_EQ(feed(line), 1u);

    const LogRecord& r = out.items[0];
    EXPECT_EQ(r.clientIp(), "192.0.2.38");
    EXPECT_EQ(r.method(), "GET");
    EXPECT_EQ(r.path(), "/contact");
    EXPECT_EQ(r.status(), 200);
    EXPECT_EQ(r.bytes(), 18224);
    EXPECT_EQ(r.raw(), line);  // raw keeps the line exactly as it was read
}

// 7. Quoted user agent containing spaces
TEST_F(ParserStageTest, QuotedUserAgentWithSpacesIsKeptWhole) {
    const std::string line =
        R"(203.0.113.54 - - [01/Sep/2026:08:01:36 +0000] "GET / HTTP/1.1" 200 512 "-" )"
        R"ua("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko)")ua";
    ASSERT_EQ(feed(line), 1u);
    EXPECT_EQ(out.items[0].userAgent(),
              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko)");
    EXPECT_FALSE(out.items[0].attribute("referer"));  // "-" means no referer
}

// 8. Query string
TEST_F(ParserStageTest, PathWithQueryStringIsKeptWhole) {
    const std::string line =
        R"(198.51.100.9 - - [01/Sep/2026:10:47:38 +0000] "GET /search?q=red+shoes&page=2 HTTP/1.1" 200 28322)";
    ASSERT_EQ(feed(line), 1u);
    EXPECT_EQ(out.items[0].path(), "/search?q=red+shoes&page=2");
}

// ---- additional cases ----

TEST_F(ParserStageTest, PlainCommonLogFormatWithoutRefererOrUserAgentIsAccepted) {
    ASSERT_EQ(feed(R"(127.0.0.1 user-identifier frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326)"), 1u);
    EXPECT_EQ(out.items[0].userAgent(), "");
    EXPECT_TRUE(out.items[0].attributes().empty());
}

TEST_F(ParserStageTest, TimeZoneOffsetIsConvertedToUtc) {
    ASSERT_EQ(feed(R"(127.0.0.1 - - [10/Oct/2000:13:55:36 -0700] "GET / HTTP/1.0" 200 2326)"), 1u);
    EXPECT_EQ(formatIso8601(out.items[0].timestamp()), "2000-10-10T20:55:36Z");
}

TEST_F(ParserStageTest, DashForBytesMeansZero) {
    ASSERT_EQ(feed(R"(192.0.2.24 - - [01/Sep/2026:10:36:17 +0000] "GET /products HTTP/1.1" 304 -)"), 1u);
    EXPECT_EQ(out.items[0].bytes(), 0);
}

TEST_F(ParserStageTest, InvalidBytesOrRequestIsSkipped) {
    EXPECT_EQ(feed(R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET / HTTP/1.1" 200 12kb)"), 0u);
    EXPECT_EQ(feed(R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "get / HTTP/1.1" 200 12)"), 0u);
    EXPECT_EQ(feed(R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET / FTP/1.1" 200 12)"), 0u);
    EXPECT_EQ(feed("this line is not an access log entry"), 0u);
    EXPECT_EQ(stage.malformedCount(), 4u);
}

TEST_F(ParserStageTest, EscapedQuoteInsideUserAgentIsAccepted) {
    ASSERT_EQ(feed(R"(192.0.2.7 - - [01/Sep/2026:09:40:02 +0000] "GET / HTTP/1.1" 200 12 "-" "Bot \"v2\"")"), 1u);
    EXPECT_EQ(out.items[0].userAgent(), R"(Bot \"v2\")");
}

TEST_F(ParserStageTest, CountsParsedAndMalformedLinesAcrossCalls) {
    feed(kValidLine);
    feed("");
    feed(kValidLine);
    feed("garbage");
    feed(kValidLine);
    EXPECT_EQ(stage.parsedCount(), 3u);
    EXPECT_EQ(stage.malformedCount(), 2u);
    EXPECT_EQ(out.items.size(), 3u);
}

TEST_F(ParserStageTest, CloseReportsMalformedCount) {
    feed(kValidLine);
    feed("garbage");
    feed("");
    stage.close();
    EXPECT_EQ(report.str(), "Malformed lines skipped: 2\n");
}

}  // namespace
}  // namespace logflow
