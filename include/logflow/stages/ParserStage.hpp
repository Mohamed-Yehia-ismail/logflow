#pragma once

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>

#include "logflow/core/Stage.hpp"
#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Parses Common Log Format lines into LogRecords:
//
//   host ident authuser [dd/Mon/yyyy:HH:MM:SS +zzzz] "METHOD path PROTOCOL" status bytes
//
// The Combined Log Format extension ("referer" "user-agent" at the end) is also
// accepted. The referer, when present, is stored in the "referer" attribute.
//
// Malformed lines are skipped and counted; nothing is emitted for them. When the
// stage is closed, it writes the count to the report stream. Proper handling of
// malformed lines (reporting what is wrong, routing them elsewhere) comes in a
// later increment; this increment deliberately only skips and counts them.
class ParserStage : public Stage<std::string, LogRecord> {
public:
    explicit ParserStage(std::ostream& report = std::cerr);

    void process(const std::string& line, Emitter<LogRecord>& out) override;
    void close() override;

    std::size_t parsedCount() const { return parsed_; }
    std::size_t malformedCount() const { return malformed_; }

private:
    static std::optional<LogRecord> parse(const std::string& line);

    std::ostream& report_;
    std::size_t parsed_ = 0;
    std::size_t malformed_ = 0;
};

}  // namespace logflow
