#pragma once

#include <stdexcept>
#include <string>

#include "logflow/model/LogRecord.hpp"
#include "logflow/stages/PredicateFilterStage.hpp"
#include "logflow/util/Timestamp.hpp"

namespace logflow::predicates {

using Predicate = PredicateFilterStage<LogRecord>::Predicate;

// status >= minimum
inline Predicate minStatus(int minimum) {
    return [minimum](const LogRecord& r) { return r.status() >= minimum; };
}

// path starts with prefix
inline Predicate pathPrefix(std::string prefix) {
    return [prefix = std::move(prefix)](const LogRecord& r) {
        return r.path().compare(0, prefix.size(), prefix) == 0;
    };
}

// from <= timestamp < to
inline Predicate timeBetween(Timestamp from, Timestamp to) {
    return [from, to](const LogRecord& r) { return r.timestamp() >= from && r.timestamp() < to; };
}

// A comparison on one field, as written in a configuration file:
//
//   field: status, op: ">=", value: 400
//
// status and bytes are numbers, timestamp is an ISO 8601 UTC time, and clientIp,
// method, path and userAgent are text.
//   numbers and timestamp: ==  !=  <  <=  >  >=
//   text:                  ==  !=  startsWith  endsWith  contains
// Throws ComparisonError with a readable message if the field, the operator or the
// value does not fit; `part` says which of the three is wrong.
struct ComparisonError : std::invalid_argument {
    ComparisonError(std::string part, const std::string& message)
        : std::invalid_argument(message), part(std::move(part)) {}
    std::string part;  // "field", "op" or "value"
};

Predicate compare(const std::string& field, const std::string& op, const std::string& value);

}  // namespace logflow::predicates
