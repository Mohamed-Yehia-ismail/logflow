// The three configurations in configs/ run with the same, unchanged program and
// produce three different outputs.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "logflow/config/BuiltinComponents.hpp"
#include "logflow/config/PipelineLoader.hpp"

namespace logflow {
namespace {

namespace fs = std::filesystem;

std::vector<std::string> readLines(const fs::path& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

class ConfigFilesTest : public ::testing::Test {
protected:
    // The configurations use paths relative to the project root.
    void SetUp() override {
        previous = fs::current_path();
        fs::current_path(LOGFLOW_SOURCE_DIR);
    }
    void TearDown() override { fs::current_path(previous); }

    std::vector<std::string> run(const std::string& config, const std::string& output) {
        fs::remove(output);
        PipelineLoader(registry).loadFile(config).pipeline.run();
        return readLines(output);
    }

    fs::path previous;
    const StageRegistry registry = builtinRegistry();
};

TEST_F(ConfigFilesTest, ErrorReport) {
    const auto lines = run("configs/error-report.yaml", "out/errors.txt");
    ASSERT_EQ(lines.size(), 74u);
    EXPECT_EQ(lines.front(), "timestamp=2026-09-01T08:01:36Z clientIp=203.0.113.54 status=404 path=/robots.txt");
}

TEST_F(ConfigFilesTest, ApiTrafficExtract) {
    const auto lines = run("configs/api-traffic.yaml", "out/api-traffic.txt");
    ASSERT_EQ(lines.size(), 21u);
    EXPECT_EQ(lines.front(), "timestamp=2026-09-01T08:03:25Z method=GET path=/api/v1/orders/1001 status=200 bytes=10389");
}

TEST_F(ConfigFilesTest, BandwidthByClientExtract) {
    const auto lines = run("configs/bandwidth-by-client.yaml", "out/bandwidth-by-client.txt");
    ASSERT_EQ(lines.size(), 177u);
    EXPECT_EQ(lines.front(), "clientIp=192.0.2.38 bytes=18224 timestamp=2026-09-01T08:01:22Z");
    for (const std::string& line : lines) EXPECT_EQ(line.find("bytes=0 "), std::string::npos) << line;
}

}  // namespace
}  // namespace logflow
