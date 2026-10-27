#pragma once

#include <cstddef>
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
// A line that cannot be parsed throws StageException with the reason (truncated
// line, invalid timestamp, not UTF-8, too long, ...). The stage does not decide what
// happens to that line: the pipeline applies the stage's error policy.
//
// Comment lines (starting with '#') are not records: they are dropped without error.
//
// The parser checks syntax only. A byte count of -45 is a well-formed number, so it is
// parsed; whether it is acceptable is decided by ValidationStage.
class ParserStage : public Stage<std::string, LogRecord> {
public:
    static constexpr std::size_t kDefaultMaxLineLength = 8 * 1024;

    // Lines longer than `maxLineLength` bytes are rejected before they are parsed.
    explicit ParserStage(std::size_t maxLineLength = kDefaultMaxLineLength);

    void process(const std::string& line, Emitter<LogRecord>& out) override;

    std::size_t parsedCount() const { return parsed_; }

private:
    LogRecord parse(const std::string& line) const;

    std::size_t maxLineLength_;
    std::size_t parsed_ = 0;
};

}  // namespace logflow
