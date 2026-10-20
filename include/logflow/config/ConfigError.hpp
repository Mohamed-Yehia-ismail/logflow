#pragma once

#include <stdexcept>
#include <string>

namespace logflow {

// A problem in a pipeline configuration, found while the pipeline is loaded.
//
// Configuration errors are reported before any data is read ("fail fast, fail
// loudly"). The message says where the problem is (file and line) and what to do.
// They are different from run-time data errors, such as a malformed log line,
// which are handled while the pipeline runs.
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace logflow
