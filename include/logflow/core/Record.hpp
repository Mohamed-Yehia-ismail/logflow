#pragma once

#include <string>

namespace logflow {

// A single unit of data flowing through a Pipeline.
//
// Increment 1 moves plain std::string lines, so nothing implements this yet.
// It fixes the project vocabulary: later increments will parse raw lines into
// structured records (timestamp, status code, path, ...) that implement it.
class Record {
public:
    virtual ~Record() = default;

    // The original, unparsed text this record was built from.
    virtual const std::string& raw() const = 0;
};

}  // namespace logflow
