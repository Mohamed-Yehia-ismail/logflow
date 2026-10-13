#include "logflow/core/PipelineBuilder.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "logflow/stages/PredicateFilterStage.hpp"

namespace logflow {
namespace {

class NumberSource : public Source<int> {
public:
    void produce(Emitter<int>& out) override {
        for (int n = 1; n <= 6; ++n) out.emit(n);
    }
};

class ToText : public Stage<int, std::string> {
public:
    void process(const int& n, Emitter<std::string>& out) override { out.emit("#" + std::to_string(n)); }
};

class TextSink : public Sink<std::string> {
public:
    explicit TextSink(std::vector<std::string>& into) : into_(into) {}
    void consume(const std::string& s) override { into_.push_back(s); }

private:
    std::vector<std::string>& into_;
};

TEST(PipelineBuilderTest, BuildsAPipelineThatRunsStagesInOrder) {
    std::vector<std::string> received;
    Pipeline pipeline =
        PipelineBuilder::from(std::make_unique<NumberSource>())
            .via(std::make_unique<PredicateFilterStage<int>>([](const int& n) { return n % 2 == 0; }))
            .via(std::make_unique<ToText>())
            .to(std::make_unique<TextSink>(received));
    pipeline.run();
    EXPECT_EQ(received, (std::vector<std::string>{"#2", "#4", "#6"}));
}

TEST(PipelineBuilderTest, StepsCanBeExtendedConditionally) {
    for (bool onlyEven : {false, true}) {
        std::vector<std::string> received;
        auto step = PipelineBuilder::from(std::make_unique<NumberSource>());
        if (onlyEven) {
            step = std::move(step).via(
                std::make_unique<PredicateFilterStage<int>>([](const int& n) { return n % 2 == 0; }));
        }
        std::move(step).via(std::make_unique<ToText>()).to(std::make_unique<TextSink>(received)).run();
        EXPECT_EQ(received.size(), onlyEven ? 3u : 6u);
    }
}

// Type mismatches are rejected at compile time by static_assert, so they cannot be
// tested at run time. For example, this does not compile:
//
//   PipelineBuilder::from(std::make_unique<NumberSource>()).to(std::make_unique<TextSink>(v));
//   // error: sink input type must match the output type of the previous component

}  // namespace
}  // namespace logflow
