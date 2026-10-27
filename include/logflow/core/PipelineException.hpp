#pragma once

#include <stdexcept>
#include <string>

namespace logflow {

// The pipeline cannot continue: an invalid configuration, an input file that cannot be
// read, an output that cannot be written (for example, a full disk), or a stage that
// failed under the FAIL_FAST policy. Not recoverable: the run stops.
//
// Compare StageException, which concerns a single item and is recoverable.
class PipelineException : public std::runtime_error {
public:
    explicit PipelineException(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace logflow
