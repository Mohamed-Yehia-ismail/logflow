// Chaos test: the deliberately corrupted file data/access-dirty.log and the empty file
// data/access-empty.log go through the configurations in configs/. The run must
// complete, the valid records must reach the main output, and every bad record must
// be accounted for.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "logflow/config/BuiltinComponents.hpp"
#include "logflow/config/PipelineLoader.hpp"
#include "logflow/core/PipelineException.hpp"

namespace logflow {
namespace {

namespace fs = std::filesystem;

std::vector<std::string> readLines(const fs::path& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

class ChaosTest : public ::testing::Test {
protected:
    void SetUp() override {
        previous = fs::current_path();
        fs::current_path(LOGFLOW_SOURCE_DIR);
    }
    void TearDown() override { fs::current_path(previous); }

    RunSummary run(const std::string& config) {
        LoadedPipeline loaded = PipelineLoader(registry).loadFile(config);
        return loaded.pipeline.run();
    }

    fs::path previous;
    const StageRegistry registry = builtinRegistry();
};

TEST_F(ChaosTest, DeadLetterPolicyKeepsValidRecordsAndRoutesEveryBadOne) {
    const RunSummary s = run("configs/dirty-dead-letter.yaml");

    const auto valid = readLines("out/dirty-valid.txt");
    ASSERT_EQ(valid.size(), 3u);
    EXPECT_NE(valid[0].find("GET /index.html  200  1234 B"), std::string::npos);
    EXPECT_NE(valid[1].find("POST /api/login  401  287 B"), std::string::npos);
    EXPECT_NE(valid[2].find("GET /images/logo.png  304  0 B"), std::string::npos);

    const auto failed = readLines("out/dead-letter.txt");
    ASSERT_EQ(failed.size(), 7u);
    const std::vector<std::pair<std::string, std::string>> expected = {
        {"record=4 stage=parse", "access-log layout"},        // truncated timestamp
        {"record=5 stage=validate", "bytes -45"},             // negative byte count
        {"record=7 stage=validate", "1970-01-01"},            // too old
        {"record=8 stage=validate", "2099-12-31"},            // in the future
        {"record=9 stage=parse", "access-log layout"},        // missing byte count
        {"record=10 stage=parse", "not valid UTF-8"},         // bytes FF FE FA
        {"record=11 stage=parse", "102400 bytes long"},       // 100 KiB line
    };
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NE(failed[i].find(expected[i].first), std::string::npos) << failed[i].substr(0, 200);
        EXPECT_NE(failed[i].find(expected[i].second), std::string::npos) << failed[i].substr(0, 200);
    }
    EXPECT_NE(failed[5].find("\\xFF\\xFE\\xFA"), std::string::npos);  // raw bytes kept, escaped

    EXPECT_TRUE(s.completed);
    EXPECT_EQ(s.recordsRead, 11u);          // 1 comment line + 10 records
    EXPECT_EQ(s.recordsWritten, 3u);
    EXPECT_EQ(s.stages[0].filtered(), 1u);  // the comment line
    EXPECT_EQ(s.deadLettered(), 7u);
    EXPECT_TRUE(s.balanced());
}

TEST_F(ChaosTest, SkipPolicyDropsAndCountsEveryBadRecord) {
    testing::internal::CaptureStdout();
    const RunSummary s = run("configs/dirty-skip.yaml");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(s.recordsWritten, 3u);
    EXPECT_EQ(s.stages[0].skipped, 4u);
    EXPECT_EQ(s.stages[1].skipped, 3u);
    EXPECT_EQ(s.deadLettered(), 0u);
    EXPECT_TRUE(s.balanced());
    EXPECT_NE(output.find("/images/logo.png"), std::string::npos);
}

TEST_F(ChaosTest, FailFastPolicyStopsAtTheFirstBadRecord) {
    LoadedPipeline loaded = PipelineLoader(registry).loadFile("configs/dirty-fail-fast.yaml");
    testing::internal::CaptureStdout();
    EXPECT_THROW(loaded.pipeline.run(), PipelineException);
    testing::internal::GetCapturedStdout();
    EXPECT_FALSE(loaded.pipeline.lastSummary().completed);
    EXPECT_EQ(loaded.pipeline.lastSummary().recordsRead, 4u);
}

TEST_F(ChaosTest, EmptyFileCompletesWithZeroRecords) {
    const RunSummary s = run("configs/empty-input.yaml");
    EXPECT_TRUE(s.completed);
    EXPECT_EQ(s.recordsRead, 0u);
    EXPECT_EQ(s.recordsWritten, 0u);
    EXPECT_TRUE(s.balanced());
    EXPECT_TRUE(readLines("out/empty-valid.txt").empty());
    EXPECT_TRUE(readLines("out/empty-dead-letter.txt").empty());
}

}  // namespace
}  // namespace logflow
