#include "logflow/app/PipelineAssembly.hpp"

#include <limits>
#include <memory>
#include <utility>

#include "logflow/config/BuiltinComponents.hpp"
#include "logflow/config/PipelineLoader.hpp"
#include "logflow/core/PipelineBuilder.hpp"
#include "logflow/io/ConsoleSink.hpp"
#include "logflow/io/FileLineSource.hpp"
#include "logflow/io/FileSink.hpp"
#include "logflow/stages/ParserStage.hpp"
#include "logflow/stages/Predicates.hpp"
#include "logflow/stages/ProjectionStage.hpp"

namespace logflow {
namespace {

// Ends the pipeline with the sink the options ask for, whatever the item type.
template <typename T>
Pipeline finish(PipelineBuilder::Step<T> step, const Options& options) {
    if (options.output) return std::move(step).to(std::make_unique<FileSink<T>>(*options.output));
    return std::move(step).to(std::make_unique<ConsoleSink<T>>());
}

}  // namespace

Pipeline assemblePipeline(const Options& options) {
    if (options.config) {
        const StageRegistry registry = builtinRegistry();
        return PipelineLoader(registry).loadFile(*options.config).pipeline;
    }
    // Without a configuration file, malformed lines are skipped (and counted in the
    // run summary), as in earlier increments.

    auto step = PipelineBuilder::from(std::make_unique<FileLineSource>(options.input))
                    .via(std::make_unique<ParserStage>(), {"parse", ErrorPolicy::skip()});

    const auto filter = [&step](predicates::Predicate predicate) {
        step = std::move(step).via(std::make_unique<PredicateFilterStage<>>(std::move(predicate)), {"filter"});
    };
    if (options.since || options.until) {
        filter(predicates::timeBetween(options.since.value_or(Timestamp::min()),
                                       options.until.value_or(Timestamp::max())));
    }
    if (options.minStatus) filter(predicates::minStatus(*options.minStatus));
    if (options.pathPrefix) filter(predicates::pathPrefix(*options.pathPrefix));

    if (options.fields) {
        return finish(std::move(step).via(std::make_unique<ProjectionStage>(*options.fields), {"project"}), options);
    }
    return finish(std::move(step), options);
}

}  // namespace logflow
