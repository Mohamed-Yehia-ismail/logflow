// A sink writes to the outside world by definition, so these tests use temporary files.

#include "logflow/io/FileSink.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace logflow {
namespace {

namespace fs = std::filesystem;

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream content;
    content << in.rdbuf();
    return content.str();
}

class FileSinkTest : public ::testing::Test {
protected:
    void TearDown() override { fs::remove(path); }

    const fs::path path = fs::temp_directory_path() / "logflow-file-sink-test.txt";
    const ProjectedRecord record{{{"status", "500"}, {"path", "/api/v1/users"}}};
};

TEST_F(FileSinkTest, WritesOneLinePerItem) {
    FileSink<ProjectedRecord> sink(path.string());
    sink.open();
    sink.consume(record);
    sink.consume(record);
    sink.close();
    EXPECT_EQ(readFile(path), "status=500 path=/api/v1/users\nstatus=500 path=/api/v1/users\n");
}

// The lesson of this increment: output is buffered, and only close() writes it out.
// A sink that is never closed silently loses everything still in the buffer.
TEST_F(FileSinkTest, OutputIsOnlyInTheFileAfterClose) {
    FileSink<ProjectedRecord> sink(path.string());
    sink.open();
    sink.consume(record);
    EXPECT_EQ(readFile(path), "") << "the line is still in the buffer";
    sink.close();
    EXPECT_EQ(readFile(path), "status=500 path=/api/v1/users\n");
}

TEST_F(FileSinkTest, OpenReplacesAnExistingFile) {
    std::ofstream(path) << "old content\n";
    FileSink<ProjectedRecord> sink(path.string());
    sink.open();
    sink.close();
    EXPECT_EQ(readFile(path), "");
}

TEST_F(FileSinkTest, FailsClearlyWhenFileCannotBeOpened) {
    FileSink<ProjectedRecord> sink("/nonexistent-dir/logflow/out.txt");
    EXPECT_THROW(sink.open(), std::runtime_error);
}

TEST_F(FileSinkTest, ConsumeBeforeOpenIsAnError) {
    FileSink<ProjectedRecord> sink(path.string());
    EXPECT_THROW(sink.consume(record), std::logic_error);
}

TEST_F(FileSinkTest, CloseWithoutOpenDoesNothing) {
    FileSink<ProjectedRecord> sink(path.string());
    EXPECT_NO_THROW(sink.close());
    EXPECT_FALSE(fs::exists(path));
}

}  // namespace
}  // namespace logflow
