#pragma once

#include "logflow/app/CommandLine.hpp"
#include "logflow/core/Pipeline.hpp"

namespace logflow {

// Turns the command-line options into a pipeline.
//
// With --config, the pipeline is loaded from the YAML file (see PipelineLoader).
// Otherwise it is assembled from the options:
//
//   FileLineSource -> ParserStage -> [filters...] -> [ProjectionStage] -> FileSink | ConsoleSink
//
// One PredicateFilterStage is added per filter option, in the order
// time range, min status, path prefix. ProjectionStage is added only with --fields.
Pipeline assemblePipeline(const Options& options);

}  // namespace logflow
