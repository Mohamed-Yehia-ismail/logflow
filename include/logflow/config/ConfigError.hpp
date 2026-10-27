#pragma once

#include <string>

#include "logflow/core/PipelineException.hpp"

namespace logflow {

// A problem in a pipeline configuration, found while the pipeline is loaded.
//
// Configuration errors are reported before any data is read ("fail fast, fail
// loudly"). The message says where the problem is (file and line) and what to do.
// A configuration error is a PipelineException: there is no pipeline to run. It is
// different from a data error, such as a malformed log line, which concerns one
// record and is handled by the stage's error policy while the pipeline runs.
class ConfigError : public PipelineException {
public:
    using PipelineException::PipelineException;
};

}  // namespace logflow
