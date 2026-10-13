// End-to-end: a small fixture file goes through a real pipeline with three stages
// (parse, filter, project) and the content of the output file is checked.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include "logflow/app/PipelineAssembly.hpp"
#include "logflow/core/PipelineBuilder.hpp"
#include "logflow/io/FileLineSource.hpp"
#include "logflow/io/FileSink.hpp"
#include "logflow/stages/ParserStage.hpp"
#include "logflow/stages/Predicates.hpp"
#include "logflow/stages/ProjectionStage.hpp"

namespace logflow {
namespace {

namespace fs = std::filesystem;

const std::string kFixture = std::string(LOGFLOW_FIXTURES_DIR) + "/mini-access.log";

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream content;
    content << in.rdbuf();
    return content.str();
}

class IntegrationTest : public ::testing::Test {
protected:
    void TearDown() override { fs::remove(out); }

    const fs::path out = fs::temp_directory_path() / "logflow-integration-test.txt";
    std::ostringstream report;
};

TEST_F(IntegrationTest, ParseFilterProjectWritesExpectedFile) {
    PipelineBuilder::from(std::make_unique<FileLineSource>(kFixture))
        .via(std::make_unique<ParserStage>(report))
        .via(std::make_unique<PredicateFilterStage<>>(predicates::minStatus(400)))
        .via(std::make_unique<ProjectionStage>(std::vector<std::string>{"timestamp", "clientIp", "status", "path"}))
        .to(std::make_unique<FileSink<ProjectedRecord>>(out.string()))
        .run();

    EXPECT_EQ(readFile(out),
              "timestamp=2026-09-01T08:00:10Z clientIp=192.0.2.11 status=500 path=/api/v1/users\n"
              "timestamp=2026-09-01T08:00:20Z clientIp=192.0.2.13 status=403 path=/admin\n"
              "timestamp=2026-09-01T08:00:25Z clientIp=192.0.2.14 status=404 path=/api/v1/orders/7\n"
              "timestamp=2026-09-01T08:00:35Z clientIp=192.0.2.16 status=503 path=\"/api/v1/users?page=2\"\n");
    EXPECT_EQ(report.str(), "Malformed lines skipped: 1\n");
}

TEST_F(IntegrationTest, CommandLineOptionsProduceTheSamePipeline) {
    const Options options = parseCommandLine({kFixture, "--min-status", "400", "--path-prefix", "/api",
                                              "--fields", "status,path", "--out", out.string()});
    assemblePipeline(options).run();

    EXPECT_EQ(readFile(out),
              "status=500 path=/api/v1/users\n"
              "status=404 path=/api/v1/orders/7\n"
              "status=503 path=\"/api/v1/users?page=2\"\n");
}

TEST_F(IntegrationTest, WithoutFieldsTheFullRecordIsWritten) {
    assemblePipeline(parseCommandLine({kFixture, "--since", "2026-09-01T08:00:30Z", "--out", out.string()})).run();

    EXPECT_EQ(readFile(out),
              "2026-09-01T08:00:30Z  192.0.2.15  GET /static/app.js  304  0 B  \"Mozilla/5.0 (X11; Linux x86_64)\"\n"
              "2026-09-01T08:00:35Z  192.0.2.16  GET /api/v1/users?page=2  503  1200 B  \"curl/8.7.1\"\n");
}

}  // namespace
}  // namespace logflow
