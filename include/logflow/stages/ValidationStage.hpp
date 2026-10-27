#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "logflow/core/Stage.hpp"
#include "logflow/model/LogRecord.hpp"
#include "logflow/util/Timestamp.hpp"

namespace logflow {

// One check on a parsed record. `check` returns std::nullopt if the record passes,
// otherwise what is wrong with it.
struct ValidationRule {
    std::string description;
    std::function<std::optional<std::string>(const LogRecord&)> check;
};

// Checks the meaning of parsed records against declarative rules, for example
// "status must be in 100..599" or "clientIp must be an IPv4 address".
//
// A record that passes every rule is emitted unchanged. A record that breaks any
// rule throws StageException listing every broken rule; the pipeline then applies
// the stage's error policy. The parser checks syntax; this stage checks meaning.
class ValidationStage : public Stage<LogRecord, LogRecord> {
public:
    explicit ValidationStage(std::vector<ValidationRule> rules);

    void process(const LogRecord& record, Emitter<LogRecord>& out) override;

private:
    std::vector<ValidationRule> rules_;
};

// Ready-made rules. Each throws std::invalid_argument if it is set up wrongly
// (for example, an unknown field name).
namespace validation {

// The field must not be empty. Fields: clientIp, method, path, userAgent.
ValidationRule required(const std::string& field);

// The numeric field must lie in [min, max]; a missing bound means unlimited.
// Fields: status, bytes.
ValidationRule range(const std::string& field, std::optional<long long> min, std::optional<long long> max);

// The timestamp must lie in [from, to]; a missing bound means unlimited.
ValidationRule timestampRange(std::optional<Timestamp> from, std::optional<Timestamp> to);

// The client address must be an IP address. Formats: ipv4, ipv6, ip (either).
ValidationRule ipFormat(const std::string& format);

// Splits a range written as "min..max" ("0..", "..599" and "100..599" are all valid).
// Returns std::nullopt if there is no "..".
std::optional<std::pair<std::string, std::string>> splitRange(const std::string& text);

}  // namespace validation

}  // namespace logflow
