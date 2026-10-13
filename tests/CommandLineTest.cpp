#include "logflow/app/CommandLine.hpp"

#include <gtest/gtest.h>

namespace logflow {
namespace {

TEST(CommandLineTest, InputFileOnly) {
    const Options o = parseCommandLine({"data/access.log"});
    EXPECT_EQ(o.input, "data/access.log");
    EXPECT_FALSE(o.output);
    EXPECT_FALSE(o.minStatus);
    EXPECT_FALSE(o.pathPrefix);
    EXPECT_FALSE(o.since);
    EXPECT_FALSE(o.until);
    EXPECT_FALSE(o.fields);
}

TEST(CommandLineTest, AllOptionsInAnyOrder) {
    const Options o = parseCommandLine({"--fields", "timestamp,clientIp,status", "--min-status", "400",
                                        "in.log", "--path-prefix", "/api", "--out", "out.txt",
                                        "--since", "2026-09-01T09:00:00Z", "--until", "2026-09-01T10:00:00Z"});
    EXPECT_EQ(o.input, "in.log");
    EXPECT_EQ(o.output, "out.txt");
    EXPECT_EQ(o.minStatus, 400);
    EXPECT_EQ(o.pathPrefix, "/api");
    EXPECT_EQ(o.since, parseIso8601("2026-09-01T09:00:00Z"));
    EXPECT_EQ(o.until, parseIso8601("2026-09-01T10:00:00Z"));
    EXPECT_EQ(o.fields, (std::vector<std::string>{"timestamp", "clientIp", "status"}));
}

TEST(CommandLineTest, Help) {
    EXPECT_TRUE(parseCommandLine({"--help"}).help);
    EXPECT_TRUE(parseCommandLine({"-h"}).help);
    EXPECT_NE(std::string(usageText()).find("--min-status"), std::string::npos);
}

TEST(CommandLineTest, InvalidCommandLinesAreRejected) {
    const std::vector<std::vector<std::string>> invalid = {
        {},                                      // no input
        {"--min-status", "400"},                 // no input
        {"a.log", "b.log"},                      // two inputs
        {"a.log", "--min-status"},               // missing value
        {"a.log", "--min-status", "abc"},        // not a number
        {"a.log", "--min-status", "4OO"},        // trailing junk
        {"a.log", "--since", "yesterday"},       // bad time
        {"a.log", "--fields", "status,,path"},   // empty field
        {"a.log", "--fields", "status,secret"},  // unknown field
        {"a.log", "--colour", "red"},            // unknown option
    };
    for (const auto& args : invalid) {
        EXPECT_THROW(parseCommandLine(args), UsageError) << ::testing::PrintToString(args);
    }
}

}  // namespace
}  // namespace logflow
