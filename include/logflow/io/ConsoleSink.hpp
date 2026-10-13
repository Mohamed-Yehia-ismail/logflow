#pragma once

#include <iostream>

#include "logflow/core/Sink.hpp"
#include "logflow/io/LineFormat.hpp"
#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Prints every item it receives as one line (see LineFormat.hpp).
// Works for any item type that has a formatLine overload, LogRecord by default.
template <typename T = LogRecord>
class ConsoleSink : public Sink<T> {
public:
    explicit ConsoleSink(std::ostream& out = std::cout) : out_(out) {}

    void consume(const T& item) override { out_ << formatLine(item) << '\n'; }

private:
    std::ostream& out_;
};

}  // namespace logflow
