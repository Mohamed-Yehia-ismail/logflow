#include "logflow/core/ErrorPolicy.hpp"

#include <gtest/gtest.h>

namespace logflow {
namespace {

TEST(ErrorPolicyTest, ParsesEveryPolicyInUpperOrLowerCase) {
    EXPECT_EQ(ErrorPolicy::parse("FAIL_FAST")->kind, ErrorPolicy::Kind::FailFast);
    EXPECT_EQ(ErrorPolicy::parse("skip")->kind, ErrorPolicy::Kind::Skip);
    EXPECT_EQ(ErrorPolicy::parse("Dead_Letter")->kind, ErrorPolicy::Kind::DeadLetter);

    const auto retry = ErrorPolicy::parse("retry(3)");
    ASSERT_TRUE(retry);
    EXPECT_EQ(retry->kind, ErrorPolicy::Kind::Retry);
    EXPECT_EQ(retry->retries, 3);
}

TEST(ErrorPolicyTest, RejectsUnknownNamesAndBadRetryCounts) {
    for (const char* text : {"", "ignore", "RETRY", "RETRY()", "RETRY(0)", "RETRY(11)", "RETRY(x)", "RETRY(3"}) {
        EXPECT_FALSE(ErrorPolicy::parse(text)) << text;
    }
}

TEST(ErrorPolicyTest, NameIsWhatTheConfigurationSays) {
    EXPECT_EQ(ErrorPolicy::failFast().name(), "FAIL_FAST");
    EXPECT_EQ(ErrorPolicy::skip().name(), "SKIP");
    EXPECT_EQ(ErrorPolicy::deadLetter().name(), "DEAD_LETTER");
    EXPECT_EQ(ErrorPolicy::retry(2).name(), "RETRY(2)");
}

TEST(ErrorPolicyTest, OnlyDeadLetterAndRetryNeedTheDeadLetterSink) {
    EXPECT_FALSE(ErrorPolicy::failFast().usesDeadLetter());
    EXPECT_FALSE(ErrorPolicy::skip().usesDeadLetter());
    EXPECT_TRUE(ErrorPolicy::deadLetter().usesDeadLetter());
    EXPECT_TRUE(ErrorPolicy::retry(1).usesDeadLetter());
}

}  // namespace
}  // namespace logflow
