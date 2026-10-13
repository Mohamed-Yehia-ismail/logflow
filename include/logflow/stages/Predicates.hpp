#pragma once

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

}  // namespace logflow::predicates
