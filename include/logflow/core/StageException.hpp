#pragma once

#include <stdexcept>
#include <string>

namespace logflow {

// Signals that a Stage failed to process an item.
class StageException : public std::runtime_error {
public:
    explicit StageException(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace logflow
