// A source reads from the outside world by definition, so unlike the stage tests
// this one uses a temporary file.

#include "logflow/io/FileLineSource.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "support/CollectingEmitter.hpp"

namespace logflow {
namespace {

namespace fs = std::filesystem;

TEST(FileLineSourceTest, EmitsOneItemPerLineAndStripsCarriageReturns) {
    const fs::path path = fs::temp_directory_path() / "logflow-file-line-source-test.log";
    {
        std::ofstream file(path, std::ios::binary);
        file << "first\r\nsecond\n\nlast";
    }

    FileLineSource source(path.string());
    test::CollectingEmitter<std::string> out;
    source.produce(out);
    fs::remove(path);

    EXPECT_EQ(out.items, (std::vector<std::string>{"first", "second", "", "last"}));
}

TEST(FileLineSourceTest, MissingFileThrows) {
    FileLineSource source("/nonexistent/logflow/missing.log");
    test::CollectingEmitter<std::string> out;
    EXPECT_THROW(source.produce(out), std::runtime_error);
}

}  // namespace
}  // namespace logflow
