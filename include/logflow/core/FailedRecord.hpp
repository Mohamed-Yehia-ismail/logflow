#pragma once

#include <cstddef>
#include <string>

#include "logflow/util/Timestamp.hpp"

namespace logflow {

// An item that a stage could not process, with what is needed to diagnose it.
// This is what travels on the dead-letter channel.
struct FailedRecord {
    std::string rawInput;       // the original input line, exactly as read
    std::string stageName;      // the stage that failed
    std::string exception;      // the reason (the StageException message)
    Timestamp timestamp;        // when it failed
    std::size_t recordNumber;   // position of the record in the input, starting at 1
};

}  // namespace logflow
