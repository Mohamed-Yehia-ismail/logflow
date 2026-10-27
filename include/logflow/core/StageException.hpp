#pragma once

#include <stdexcept>
#include <string>

namespace logflow {

// A stage could not process one item. Recoverable: the pipeline applies the stage's
// error policy (see ErrorPolicy.hpp) to that one item and continues with the next.
//
// Compare PipelineException, which means the whole run cannot continue.
class StageException : public std::runtime_error {
public:
    explicit StageException(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace logflow
