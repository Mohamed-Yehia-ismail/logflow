#include "logflow/core/Pipeline.hpp"

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

class FailingStage : public Stage<std::string, std::string> {
public:
    explicit FailingStage(Log& log) : log_(log) {}
    void process(const std::string&, Emitter<std::string>&) override { throw StageException("boom"); }
    void close() override { log_.push_back("close failing"); }

private:
    Log& log_;
};

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
    pipeline.addStage(std::make_unique<FailingStage>(log))
        .setSink(std::make_unique<ListSink<std::string>>(received));

    EXPECT_THROW(pipeline.run(), StageException);
    EXPECT_TRUE(received.empty());
    EXPECT_EQ(log, (Log{"close failing"}));
}

}  // namespace
}  // namespace logflow
