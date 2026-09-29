#pragma once

#include <iostream>
#include <string>

#include "logflow/core/Sink.hpp"

namespace logflow {

// Prints every item it receives on its own line.
class ConsoleSink : public Sink<std::string> {
public:
    explicit ConsoleSink(std::ostream& out = std::cout) : out_(out) {}

    void consume(const std::string& item) override { out_ << item << '\n'; }

private:
    std::ostream& out_;
};

}  // namespace logflow
