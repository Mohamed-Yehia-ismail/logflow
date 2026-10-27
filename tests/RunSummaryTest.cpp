#include "logflow/core/RunSummary.hpp"

#include <gtest/gtest.h>

namespace logflow {
namespace {

RunSummary example() {
    RunSummary s;
    s.pipelineName = "demo";
    s.recordsRead = 11;
    s.recordsWritten = 3;
    StageStats parse{"parse", "DEAD_LETTER", 11, 6, 0, 4, 0};
    StageStats validate{"validate", "DEAD_LETTER", 6, 3, 0, 3, 0};
    s.stages = {parse, validate};
    s.completed = true;
    return s;
}

TEST(RunSummaryTest, CountsAddUp) {
    const RunSummary s = example();
    EXPECT_EQ(s.stages[0].filtered(), 1u);
    EXPECT_EQ(s.dropped(), 1u);
    EXPECT_EQ(s.deadLettered(), 7u);
    EXPECT_TRUE(s.balanced());
}

TEST(RunSummaryTest, MismatchIsDetected) {
    RunSummary s = example();
    s.recordsRead = 12;
    EXPECT_FALSE(s.balanced());
    EXPECT_NE(formatSummary(s).find("MISMATCH"), std::string::npos);
}

TEST(RunSummaryTest, FormatShowsEveryStageAndTheCheck) {
    RunSummary s = example();
    s.stages[1].retries = 2;
    const std::string text = formatSummary(s);
    EXPECT_EQ(text.rfind("Run summary: demo\n", 0), 0u);
    EXPECT_NE(text.find("1. parse"), std::string::npos);
    EXPECT_NE(text.find("2. validate"), std::string::npos);
    EXPECT_NE(text.find("(retries: 2)"), std::string::npos);
    EXPECT_NE(text.find("check: 11 read = 3 written + 1 dropped + 7 dead-lettered  OK"), std::string::npos);
}

TEST(RunSummaryTest, StoppedRunHasNoCheckLine) {
    RunSummary s = example();
    s.completed = false;
    s.pipelineName.clear();
    const std::string text = formatSummary(s);
    EXPECT_EQ(text.rfind("Run summary: pipeline (stopped early)\n", 0), 0u);
    EXPECT_EQ(text.find("check:"), std::string::npos);
}

TEST(RunSummaryTest, StageThatEmitsMoreThanItReceivesHasNothingFiltered) {
    StageStats split{"split", "SKIP", 1, 3, 0, 0, 0};
    EXPECT_EQ(split.filtered(), 0u);
}

}  // namespace
}  // namespace logflow
