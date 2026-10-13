#pragma once

#include "logflow/app/CommandLine.hpp"
#include "logflow/core/Pipeline.hpp"

namespace logflow {

// Turns the command-line options into a pipeline:
//
//   FileLineSource -> ParserStage -> [filters...] -> [ProjectionStage] -> FileSink | ConsoleSink
//
// One PredicateFilterStage is added per filter option, in the order
// time range, min status, path prefix. ProjectionStage is added only with --fields.
// (Increment 4 replaces this hand-written assembly with configuration.)
Pipeline assemblePipeline(const Options& options);

}  // namespace logflow
