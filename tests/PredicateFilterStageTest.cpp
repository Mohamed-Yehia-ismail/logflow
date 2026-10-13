#include "logflow/stages/PredicateFilterStage.hpp"

#include <gtest/gtest.h>

#include "logflow/stages/Predicates.hpp"
#include "support/CollectingEmitter.hpp"

namespace logflow {
namespace {

using test::CollectingEmitter;

LogRecord record(int status, const std::string& path, const std::string& isoTime = "2026-09-01T09:00:00Z") {
    return LogRecord::Builder().status(status).path(path).timestamp(*parseIso8601(isoTime)).build();
}

// Runs the records through the filter and returns the paths of those that pass.
std::vector<std::string> pathsPassing(predicates::Predicate predicate, const std::vector<LogRecord>& in) {
    PredicateFilterStage<> stage(std::move(predicate));
    CollectingEmitter<LogRecord> out;
    for (const LogRecord& r : in) stage.process(r, out);
    std::vector<std::string> paths;
    for (const LogRecord& r : out.items) paths.push_back(r.path());
    return paths;
}

TEST(PredicateFilterStageTest, PassesItemsUnchangedWhenPredicateAccepts) {
    PredicateFilterStage<> stage([](const LogRecord&) { return true; });
    CollectingEmitter<LogRecord> out;
    const LogRecord r = record(200, "/a");
    stage.process(r, out);
    ASSERT_EQ(out.items.size(), 1u);
    EXPECT_EQ(out.items[0].path(), "/a");
    EXPECT_EQ(out.items[0].status(), 200);
}

TEST(PredicateFilterStageTest, DropsItemsWhenPredicateRejects) {
    EXPECT_TRUE(pathsPassing([](const LogRecord&) { return false; }, {record(200, "/a")}).empty());
}

TEST(PredicateFilterStageTest, MinStatusKeepsStatusAtOrAboveTheLimit) {
    const auto passed = pathsPassing(predicates::minStatus(400),
                                     {record(399, "/399"), record(400, "/400"), record(503, "/503")});
    EXPECT_EQ(passed, (std::vector<std::string>{"/400", "/503"}));
}

TEST(PredicateFilterStageTest, PathPrefixKeepsMatchingPaths) {
    const auto passed = pathsPassing(predicates::pathPrefix("/api"),
                                     {record(200, "/api/v1/users"), record(200, "/apix"),
                                      record(200, "/v1/api"), record(200, "/ap"), record(200, "/api")});
    EXPECT_EQ(passed, (std::vector<std::string>{"/api/v1/users", "/apix", "/api"}));
}

TEST(PredicateFilterStageTest, TimeBetweenIncludesStartAndExcludesEnd) {
    const auto from = *parseIso8601("2026-09-01T09:00:00Z");
    const auto to = *parseIso8601("2026-09-01T10:00:00Z");
    const auto passed = pathsPassing(predicates::timeBetween(from, to),
                                     {record(200, "/before", "2026-09-01T08:59:59Z"),
                                      record(200, "/start", "2026-09-01T09:00:00Z"),
                                      record(200, "/inside", "2026-09-01T09:30:00Z"),
                                      record(200, "/end", "2026-09-01T10:00:00Z")});
    EXPECT_EQ(passed, (std::vector<std::string>{"/start", "/inside"}));
}

TEST(PredicateFilterStageTest, WorksForOtherItemTypes) {
    PredicateFilterStage<int> evens([](const int& n) { return n % 2 == 0; });
    CollectingEmitter<int> out;
    for (int n = 1; n <= 6; ++n) evens.process(n, out);
    EXPECT_EQ(out.items, (std::vector<int>{2, 4, 6}));
}

TEST(PredicateFilterStageTest, RejectsEmptyPredicate) {
    EXPECT_THROW(PredicateFilterStage<>(nullptr), std::invalid_argument);
}

}  // namespace
}  // namespace logflow
