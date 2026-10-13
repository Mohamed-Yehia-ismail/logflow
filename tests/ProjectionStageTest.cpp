#include "logflow/stages/ProjectionStage.hpp"

#include <gtest/gtest.h>

#include "logflow/util/Timestamp.hpp"
#include "support/CollectingEmitter.hpp"

namespace logflow {
namespace {

using test::CollectingEmitter;
using Fields = std::vector<ProjectedRecord::Field>;

LogRecord sample() {
    return LogRecord::Builder()
        .timestamp(*parseIso8601("2026-09-01T08:01:22Z"))
        .clientIp("192.0.2.38")
        .method("GET")
        .path("/contact")
        .status(404)
        .bytes(18224)
        .userAgent("curl/8.7.1")
        .raw("the whole original line")
        .build();
}

ProjectedRecord project(std::vector<std::string> fields) {
    ProjectionStage stage(std::move(fields));
    CollectingEmitter<ProjectedRecord> out;
    stage.process(sample(), out);
    EXPECT_EQ(out.items.size(), 1u);
    return out.items.at(0);
}

TEST(ProjectionStageTest, KeepsOnlySelectedFieldsInRequestedOrder) {
    EXPECT_EQ(project({"status", "timestamp", "clientIp"}).fields(),
              (Fields{{"status", "404"}, {"timestamp", "2026-09-01T08:01:22Z"}, {"clientIp", "192.0.2.38"}}));
}

TEST(ProjectionStageTest, CanSelectEveryField) {
    EXPECT_EQ(project({"timestamp", "clientIp", "method", "path", "status", "bytes", "userAgent"}).fields(),
              (Fields{{"timestamp", "2026-09-01T08:01:22Z"},
                      {"clientIp", "192.0.2.38"},
                      {"method", "GET"},
                      {"path", "/contact"},
                      {"status", "404"},
                      {"bytes", "18224"},
                      {"userAgent", "curl/8.7.1"}}));
}

TEST(ProjectionStageTest, DroppedFieldsAreGone) {
    const ProjectedRecord p = project({"path"});
    EXPECT_EQ(p.value("path"), "/contact");
    EXPECT_FALSE(p.value("clientIp"));
    EXPECT_FALSE(p.value("raw"));
}

TEST(ProjectionStageTest, EmitsExactlyOneOutputPerInput) {
    ProjectionStage stage({"status"});
    CollectingEmitter<ProjectedRecord> out;
    for (int i = 0; i < 3; ++i) stage.process(sample(), out);
    EXPECT_EQ(out.items.size(), 3u);
}

TEST(ProjectionStageTest, RejectsUnknownOrNoFields) {
    EXPECT_THROW(ProjectionStage({"status", "country"}), std::invalid_argument);
    EXPECT_THROW(ProjectionStage({"raw"}), std::invalid_argument);
    EXPECT_THROW(ProjectionStage(std::vector<std::string>{}), std::invalid_argument);
}

}  // namespace
}  // namespace logflow
