#pragma once

#include <iostream>

#include "logflow/core/Sink.hpp"
#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Prints every record it receives as one readable line, for example:
//
//   2026-09-01T08:01:22Z  192.0.2.38  GET /contact  200  18224 B  "Mozilla/5.0 ..."
class ConsoleSink : public Sink<LogRecord> {
public:
    explicit ConsoleSink(std::ostream& out = std::cout) : out_(out) {}

    void consume(const LogRecord& record) override;

private:
    std::ostream& out_;
};

}  // namespace logflow
