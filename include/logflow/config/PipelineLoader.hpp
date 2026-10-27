#pragma once

#include <string>

#include "logflow/config/StageRegistry.hpp"
#include "logflow/core/Pipeline.hpp"

namespace logflow {

// A pipeline built from a configuration, with the name given in the configuration.
struct LoadedPipeline {
    std::string name;
    Pipeline pipeline;
};

// Reads a YAML pipeline configuration and builds the Pipeline it describes:
//
//   pipeline:
//     name: error-report
//     source: { type: file, params: { path: data/access-small.log } }
//     stages:
//       - type: parse
//         onError: DEAD_LETTER
//       - type: filter
//         params: { field: status, op: ">=", value: 400 }
//     sink: { type: file, params: { path: out/errors.txt } }
//     deadLetter: { type: file, params: { path: out/dead-letter.txt } }
//
// onError (FAIL_FAST, SKIP, DEAD_LETTER, RETRY(n)) is optional and defaults to SKIP.
// deadLetter is optional, but required when a stage uses DEAD_LETTER or RETRY(n).
//
// Loading happens in three steps, all before any data is read:
//   1. read:     the YAML is parsed and checked against the schema
//                (required keys, no unknown keys, correct shapes);
//   2. resolve:  every type name is looked up in the registry and the item types
//                are checked to fit together from source to sink;
//   3. build:    every factory validates its parameters and creates its component.
// Any problem stops loading with a ConfigError that says where it is and how to fix it.
class PipelineLoader {
public:
    explicit PipelineLoader(const StageRegistry& registry) : registry_(registry) {}

    LoadedPipeline loadFile(const std::string& path) const;

    // `sourceName` is used in error messages in place of a file name.
    LoadedPipeline loadString(const std::string& yaml, const std::string& sourceName = "<config>") const;

private:
    const StageRegistry& registry_;
};

}  // namespace logflow
