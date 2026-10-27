#include "logflow/core/Pipeline.hpp"

#include "logflow/core/PipelineException.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace logflow {
namespace {

using Log = std::vector<std::string>;

class ListSource : public Source<std::string> {
public:
    explicit ListSource(std::vector<std::string> items) : items_(std::move(items)) {}
    void produce(Emitter<std::string>& out) override {
        for (const auto& item : items_) out.emit(item);
    }

private:
    std::vector<std::string> items_;
};

template <typename T>
class ListSink : public Sink<T> {
public:
    explicit ListSink(std::vector<T>& into) : into_(into) {}
    void consume(const T& item) override { into_.push_back(item); }

private:
    std::vector<T>& into_;
};

// Drops one specific item, records lifecycle calls.
class DropStage : public Stage<std::string, std::string> {
public:
    DropStage(std::string drop, Log& log) : drop_(std::move(drop)), log_(log) {}
    void process(const std::string& in, Emitter<std::string>& out) override {
        if (in != drop_) out.emit(in);
    }
    void open() override { log_.push_back("open drop"); }
    void close() override { log_.push_back("close drop"); }

private:
    std::string drop_;
    Log& log_;
};

// Emits each item's length twice: changes the type and the number of items.
class LengthTwiceStage : public Stage<std::string, int> {
public:
    explicit LengthTwiceStage(Log& log) : log_(log) {}
    void process(const std::string& in, Emitter<int>& out) override {
        out.emit(static_cast<int>(in.size()));
        out.emit(static_cast<int>(in.size()));
    }
    void open() override { log_.push_back("open length"); }
    void close() override { log_.push_back("close length"); }

private:
    Log& log_;
};

// Records its lifecycle calls.
class LoggingSink : public Sink<std::string> {
public:
    explicit LoggingSink(Log& log) : log_(log) {}
    void consume(const std::string& item) override { log_.push_back("consume " + item); }
    void open() override { log_.push_back("open sink"); }
    void close() override { log_.push_back("close sink"); }

private:
    Log& log_;
};

class FailingStage : public Stage<std::string, std::string> {
public:
    explicit FailingStage(Log& log) : log_(log) {}
    void process(const std::string&, Emitter<std::string>&) override { throw StageException("boom"); }
    void close() override { log_.push_back("close failing"); }

private:
    Log& log_;
};

// Fails on items that start with "bad".
class PickyStage : public Stage<std::string, std::string> {
public:
    void process(const std::string& in, Emitter<std::string>& out) override {
        if (in.rfind("bad", 0) == 0) throw StageException("bad item " + in);
        out.emit(in);
    }
};

// Fails the first `failures` attempts on every item, then succeeds.
class FlakyStage : public Stage<std::string, std::string> {
public:
    explicit FlakyStage(int failures) : failures_(failures) {}
    void process(const std::string& in, Emitter<std::string>& out) override {
        if (attempts_++ < failures_) throw StageException("temporary failure");
        attempts_ = 0;
        out.emit(in);
    }

private:
    int failures_;
    int attempts_ = 0;
};

// Throws an error that is not a StageException.
class BrokenStage : public Stage<std::string, std::string> {
public:
    void process(const std::string&, Emitter<std::string>&) override { throw std::runtime_error("bug"); }
};

std::unique_ptr<ListSource> items(std::vector<std::string> list) {
    return std::make_unique<ListSource>(std::move(list));
}

TEST(PipelineTest, WithoutStagesSourceFeedsSinkDirectly) {
    std::vector<std::string> received;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a", "b"}));
    pipeline.setSink(std::make_unique<ListSink<std::string>>(received));
    pipeline.run();
    EXPECT_EQ(received, (std::vector<std::string>{"a", "b"}));
}

TEST(PipelineTest, StagesRunInOrderAndMayChangeTypeAndCount) {
    Log log;
    std::vector<int> received;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a", "bb", "ccc"}));
    pipeline.addStage(std::make_unique<DropStage>("bb", log))
        .addStage(std::make_unique<LengthTwiceStage>(log))
        .setSink(std::make_unique<ListSink<int>>(received));
    pipeline.run();

    EXPECT_EQ(received, (std::vector<int>{1, 1, 3, 3}));
    EXPECT_EQ(log, (Log{"open drop", "open length", "close length", "close drop"}));
}

TEST(PipelineTest, RejectsStageOrSinkWhoseInputTypeDoesNotMatch) {
    std::vector<int> ints;
    Log log;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{}));
    EXPECT_THROW(pipeline.setSink(std::make_unique<ListSink<int>>(ints)), std::logic_error);

    pipeline.addStage(std::make_unique<LengthTwiceStage>(log));
    EXPECT_THROW(pipeline.addStage(std::make_unique<DropStage>("x", log)), std::logic_error);
}

TEST(PipelineTest, RejectsNullComponents) {
    EXPECT_THROW(Pipeline(std::unique_ptr<ListSource>()), std::invalid_argument);

    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{}));
    EXPECT_THROW(pipeline.addStage(std::unique_ptr<DropStage>()), std::invalid_argument);
    EXPECT_THROW(pipeline.setSink(std::unique_ptr<ListSink<std::string>>()), std::invalid_argument);
}

TEST(PipelineTest, RunWithoutSinkFails) {
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a"}));
    EXPECT_THROW(pipeline.run(), std::logic_error);
}

TEST(PipelineTest, StageFailureStopsRunAndStillClosesStages) {
    Log log;
    std::vector<std::string> received;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a", "b"}));
    pipeline.addStage(std::make_unique<FailingStage>(log), {"failing", ErrorPolicy::failFast()})
        .setSink(std::make_unique<ListSink<std::string>>(received));

    EXPECT_THROW(pipeline.run(), PipelineException);
    EXPECT_FALSE(pipeline.lastSummary().completed);
    EXPECT_TRUE(received.empty());
    EXPECT_EQ(log, (Log{"close failing"}));
}

TEST(PipelineTest, SinkIsOpenedFirstAndClosedLast) {
    Log log;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a"}));
    pipeline.addStage(std::make_unique<DropStage>("x", log)).setSink(std::make_unique<LoggingSink>(log));
    pipeline.run();
    EXPECT_EQ(log, (Log{"open sink", "open drop", "consume a", "close drop", "close sink"}));
}

TEST(PipelineTest, SinkIsClosedEvenWhenAStageFails) {
    Log log;
    Pipeline pipeline(std::make_unique<ListSource>(std::vector<std::string>{"a"}));
    pipeline.addStage(std::make_unique<FailingStage>(log), {"failing", ErrorPolicy::failFast()})
        .setSink(std::make_unique<LoggingSink>(log));
    EXPECT_THROW(pipeline.run(), PipelineException);
    EXPECT_EQ(log, (Log{"open sink", "close failing", "close sink"}));
}

// ---- error policies (Increment 5) ----

TEST(PipelineTest, FailFastStopsAtTheFirstFailureAndNamesTheStageAndRecord) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"a", "bad1", "b"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::failFast()})
        .setSink(std::make_unique<ListSink<std::string>>(received));
    try {
        pipeline.run();
        FAIL() << "expected PipelineException";
    } catch (const PipelineException& e) {
        EXPECT_STREQ(e.what(), "stage 1 (picky) failed on record 2 under FAIL_FAST: bad item bad1");
    }
    EXPECT_EQ(received, (std::vector<std::string>{"a"}));
    EXPECT_EQ(pipeline.lastSummary().recordsRead, 2u);
}

TEST(PipelineTest, SkipDropsFailedItemsCountsThemAndContinues) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"a", "bad1", "b", "bad2"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::skip()})
        .setSink(std::make_unique<ListSink<std::string>>(received));
    const RunSummary summary = pipeline.run();

    EXPECT_EQ(received, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(summary.stages[0].skipped, 2u);
    EXPECT_EQ(summary.recordsWritten, 2u);
    EXPECT_TRUE(summary.balanced());
}

TEST(PipelineTest, DefaultPolicyIsSkipAndDefaultNameIsTheStagePosition) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"bad"}));
    pipeline.addStage(std::make_unique<PickyStage>()).setSink(std::make_unique<ListSink<std::string>>(received));
    const RunSummary summary = pipeline.run();
    EXPECT_EQ(summary.stages[0].name, "stage 1");
    EXPECT_EQ(summary.stages[0].policy, "SKIP");
    EXPECT_EQ(summary.stages[0].skipped, 1u);
}

TEST(PipelineTest, DeadLetterRoutesFailedItemsToTheSecondSink) {
    std::vector<std::string> received;
    std::vector<FailedRecord> failed;
    Pipeline pipeline(items({"a", "bad1", "b"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::deadLetter()})
        .setSink(std::make_unique<ListSink<std::string>>(received))
        .setDeadLetterSink(std::make_unique<ListSink<FailedRecord>>(failed));
    const RunSummary summary = pipeline.run();

    EXPECT_EQ(received, (std::vector<std::string>{"a", "b"}));
    ASSERT_EQ(failed.size(), 1u);
    EXPECT_EQ(failed[0].rawInput, "bad1");
    EXPECT_EQ(failed[0].stageName, "picky");
    EXPECT_EQ(failed[0].exception, "bad item bad1");
    EXPECT_EQ(failed[0].recordNumber, 2u);
    EXPECT_EQ(summary.deadLettered(), 1u);
    EXPECT_TRUE(summary.balanced());
}

TEST(PipelineTest, DeadLetterSinkIsOpenedAndClosedWithTheMainSink) {
    Log log;
    Pipeline pipeline(items({"a"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::deadLetter()})
        .setSink(std::make_unique<LoggingSink>(log));
    class LoggingDeadLetter : public Sink<FailedRecord> {
    public:
        explicit LoggingDeadLetter(Log& log) : log_(log) {}
        void consume(const FailedRecord&) override {}
        void open() override { log_.push_back("open dead-letter"); }
        void close() override { log_.push_back("close dead-letter"); }

    private:
        Log& log_;
    };
    pipeline.setDeadLetterSink(std::make_unique<LoggingDeadLetter>(log));
    pipeline.run();
    EXPECT_EQ(log, (Log{"open sink", "open dead-letter", "consume a", "close dead-letter", "close sink"}));
}

TEST(PipelineTest, DeadLetterPolicyWithoutADeadLetterSinkIsAProgrammingError) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"a"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::deadLetter()})
        .setSink(std::make_unique<ListSink<std::string>>(received));
    EXPECT_THROW(pipeline.run(), std::logic_error);
    EXPECT_THROW(pipeline.setDeadLetterSink(std::unique_ptr<ListSink<FailedRecord>>()), std::invalid_argument);
}

TEST(PipelineTest, RetrySucceedsWhenTheFailureIsTemporary) {
    std::vector<std::string> received;
    std::vector<FailedRecord> failed;
    Pipeline pipeline(items({"a", "b"}));
    pipeline.addStage(std::make_unique<FlakyStage>(2), {"flaky", ErrorPolicy::retry(3)})
        .setSink(std::make_unique<ListSink<std::string>>(received))
        .setDeadLetterSink(std::make_unique<ListSink<FailedRecord>>(failed));
    const RunSummary summary = pipeline.run();

    EXPECT_EQ(received, (std::vector<std::string>{"a", "b"}));
    EXPECT_TRUE(failed.empty());
    EXPECT_EQ(summary.stages[0].retries, 4u);
}

TEST(PipelineTest, RetryDeadLettersTheItemWhenRetriesRunOut) {
    std::vector<std::string> received;
    std::vector<FailedRecord> failed;
    Pipeline pipeline(items({"a"}));
    pipeline.addStage(std::make_unique<FlakyStage>(5), {"flaky", ErrorPolicy::retry(2)})
        .setSink(std::make_unique<ListSink<std::string>>(received))
        .setDeadLetterSink(std::make_unique<ListSink<FailedRecord>>(failed));
    const RunSummary summary = pipeline.run();

    EXPECT_TRUE(received.empty());
    ASSERT_EQ(failed.size(), 1u);
    EXPECT_EQ(summary.stages[0].retries, 2u);
    EXPECT_EQ(summary.stages[0].deadLettered, 1u);
}

TEST(PipelineTest, ErrorsOtherThanStageExceptionAlwaysStopTheRun) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"a"}));
    pipeline.addStage(std::make_unique<BrokenStage>(), {"broken", ErrorPolicy::skip()})
        .setSink(std::make_unique<ListSink<std::string>>(received));
    EXPECT_THROW(pipeline.run(), std::runtime_error);
}

TEST(PipelineTest, EachStageHandlesOnlyItsOwnFailures) {
    Log log;
    std::vector<std::string> received;
    std::vector<FailedRecord> failed;
    Pipeline pipeline(items({"a", "bad1", "bb"}));
    pipeline.addStage(std::make_unique<DropStage>("a", log), {"drop", ErrorPolicy::failFast()})
        .addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::deadLetter()})
        .setSink(std::make_unique<ListSink<std::string>>(received))
        .setDeadLetterSink(std::make_unique<ListSink<FailedRecord>>(failed));
    const RunSummary summary = pipeline.run();

    EXPECT_EQ(received, (std::vector<std::string>{"bb"}));
    ASSERT_EQ(failed.size(), 1u);
    EXPECT_EQ(failed[0].stageName, "picky");
    EXPECT_EQ(summary.stages[0].filtered(), 1u);   // "a" dropped on purpose
    EXPECT_EQ(summary.stages[0].deadLettered, 0u);
    EXPECT_EQ(summary.stages[1].deadLettered, 1u);
    EXPECT_TRUE(summary.balanced());
}

TEST(PipelineTest, SummaryCountsReadWrittenAndTime) {
    std::vector<std::string> received;
    Pipeline pipeline(items({"a", "b", "c"}));
    pipeline.setName("demo");
    pipeline.setSink(std::make_unique<ListSink<std::string>>(received));
    const RunSummary summary = pipeline.run();
    EXPECT_EQ(pipeline.name(), "demo");
    EXPECT_EQ(summary.pipelineName, "demo");
    EXPECT_EQ(summary.recordsRead, 3u);
    EXPECT_EQ(summary.recordsWritten, 3u);
    EXPECT_TRUE(summary.completed);
    EXPECT_GE(summary.elapsed.count(), 0);
}

TEST(PipelineTest, DeadLetterSinkIsClosedWhenTheRunFails) {
    Log log;
    std::vector<FailedRecord> failed;
    class ClosingDeadLetter : public ListSink<FailedRecord> {
    public:
        ClosingDeadLetter(std::vector<FailedRecord>& into, Log& log) : ListSink(into), log_(log) {}
        void close() override { log_.push_back("close dead-letter"); }

    private:
        Log& log_;
    };
    Pipeline pipeline(items({"bad1", "x"}));
    pipeline.addStage(std::make_unique<PickyStage>(), {"picky", ErrorPolicy::deadLetter()})
        .addStage(std::make_unique<FailingStage>(log), {"failing", ErrorPolicy::failFast()})
        .setSink(std::make_unique<LoggingSink>(log))
        .setDeadLetterSink(std::make_unique<ClosingDeadLetter>(failed, log));
    EXPECT_THROW(pipeline.run(), PipelineException);
    EXPECT_EQ(failed.size(), 1u);
    EXPECT_EQ(log, (Log{"open sink", "close failing", "close dead-letter", "close sink"}));
}

// Items that are neither text nor a Record have no raw text to keep.
TEST(PipelineTest, DeadLetteredItemWithoutRawTextHasEmptyRawInput) {
    class RejectAll : public Stage<int, int> {
    public:
        void process(const int&, Emitter<int>&) override { throw StageException("no"); }
    };
    Log log;
    std::vector<int> received;
    std::vector<FailedRecord> failed;
    Pipeline pipeline(items({"a"}));
    pipeline.addStage(std::make_unique<LengthTwiceStage>(log))
        .addStage(std::make_unique<RejectAll>(), {"reject", ErrorPolicy::deadLetter()})
        .setSink(std::make_unique<ListSink<int>>(received))
        .setDeadLetterSink(std::make_unique<ListSink<FailedRecord>>(failed));
    pipeline.run();
    ASSERT_EQ(failed.size(), 2u);
    EXPECT_EQ(failed[0].rawInput, "");
}

}  // namespace
}  // namespace logflow
