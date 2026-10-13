#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "logflow/core/Pipeline.hpp"

namespace logflow {

// Fluent, type-checked assembly of a Pipeline:
//
//   Pipeline p = PipelineBuilder::from(std::make_unique<FileLineSource>(in))
//                    .via(std::make_unique<ParserStage>())
//                    .via(std::make_unique<PredicateFilterStage<>>(predicates::minStatus(400)))
//                    .via(std::make_unique<ProjectionStage>(fields))
//                    .to(std::make_unique<FileSink<ProjectedRecord>>(out));
//
// Each step knows the item type flowing at that point, so connecting a stage or
// sink that expects a different type is a compile error, not a runtime one.
// Each call consumes the step it is called on (note the && qualifiers): a step
// that is stored in a variable is continued with std::move(step).via(...).
class PipelineBuilder {
public:
    template <typename T>
    class Step;

    template <typename S>
    static Step<typename S::OutputType> from(std::unique_ptr<S> source) {
        return Step<typename S::OutputType>(Pipeline(std::move(source)));
    }
};

template <typename T>
class PipelineBuilder::Step {
public:
    // Appends a stage that accepts T.
    template <typename S>
    Step<typename S::OutputType> via(std::unique_ptr<S> stage) && {
        static_assert(std::is_same_v<typename S::InputType, T>,
                      "stage input type must match the output type of the previous component");
        pipeline_.addStage(std::move(stage));
        return Step<typename S::OutputType>(std::move(pipeline_));
    }

    // Ends the pipeline with a sink that accepts T.
    template <typename S>
    Pipeline to(std::unique_ptr<S> sink) && {
        static_assert(std::is_same_v<typename S::InputType, T>,
                      "sink input type must match the output type of the previous component");
        pipeline_.setSink(std::move(sink));
        return std::move(pipeline_);
    }

private:
    friend class PipelineBuilder;
    template <typename>
    friend class Step;

    explicit Step(Pipeline pipeline) : pipeline_(std::move(pipeline)) {}

    Pipeline pipeline_;
};

}  // namespace logflow
