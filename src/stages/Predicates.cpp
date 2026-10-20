#include "logflow/stages/Predicates.hpp"

#include <functional>

namespace logflow::predicates {
namespace {

const char* const kOrderOps = "==, !=, <, <=, >, >=";
const char* const kTextOps = "==, !=, startsWith, endsWith, contains";

// Builds a predicate for an ordered value (number or time) read from the record by `get`.
template <typename T>
Predicate ordered(const std::string& field, std::function<T(const LogRecord&)> get, const std::string& op, T limit) {
    if (op == "==") return [get, limit](const LogRecord& r) { return get(r) == limit; };
    if (op == "!=") return [get, limit](const LogRecord& r) { return get(r) != limit; };
    if (op == "<") return [get, limit](const LogRecord& r) { return get(r) < limit; };
    if (op == "<=") return [get, limit](const LogRecord& r) { return get(r) <= limit; };
    if (op == ">") return [get, limit](const LogRecord& r) { return get(r) > limit; };
    if (op == ">=") return [get, limit](const LogRecord& r) { return get(r) >= limit; };
    throw ComparisonError("op", "operator '" + op + "' is not supported for field '" + field + "' (use " +
                                    kOrderOps + ").");
}

Predicate text(const std::string& field, std::function<const std::string&(const LogRecord&)> get,
               const std::string& op, const std::string& value) {
    if (op == "==") return [get, value](const LogRecord& r) { return get(r) == value; };
    if (op == "!=") return [get, value](const LogRecord& r) { return get(r) != value; };
    if (op == "startsWith") {
        return [get, value](const LogRecord& r) { return get(r).compare(0, value.size(), value) == 0; };
    }
    if (op == "endsWith") {
        return [get, value](const LogRecord& r) {
            const std::string& s = get(r);
            return s.size() >= value.size() && s.compare(s.size() - value.size(), value.size(), value) == 0;
        };
    }
    if (op == "contains") {
        return [get, value](const LogRecord& r) { return get(r).find(value) != std::string::npos; };
    }
    throw ComparisonError("op", "operator '" + op + "' is not supported for text field '" + field + "' (use " +
                                    kTextOps + ").");
}

long long wholeNumber(const std::string& field, const std::string& value) {
    std::size_t used = 0;
    long long number = 0;
    try {
        number = std::stoll(value, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    if (used == 0 || used != value.size()) {
        throw ComparisonError("value", "value '" + value + "' for field '" + field + "' must be a whole number.");
    }
    return number;
}

}  // namespace

Predicate compare(const std::string& field, const std::string& op, const std::string& value) {
    if (field == "status") {
        const long long limit = wholeNumber(field, value);
        return ordered<long long>(field, [](const LogRecord& r) { return static_cast<long long>(r.status()); }, op, limit);
    }
    if (field == "bytes") {
        const long long limit = wholeNumber(field, value);
        return ordered<long long>(field, [](const LogRecord& r) { return r.bytes(); }, op, limit);
    }
    if (field == "timestamp") {
        const std::optional<Timestamp> time = parseIso8601(value);
        if (!time) {
            throw ComparisonError("value", "value '" + value +
                                               "' for field 'timestamp' must be a UTC time like 2026-09-01T09:00:00Z.");
        }
        return ordered<Timestamp>(field, [](const LogRecord& r) { return r.timestamp(); }, op, *time);
    }
    if (field == "clientIp") return text(field, [](const LogRecord& r) -> const std::string& { return r.clientIp(); }, op, value);
    if (field == "method") return text(field, [](const LogRecord& r) -> const std::string& { return r.method(); }, op, value);
    if (field == "path") return text(field, [](const LogRecord& r) -> const std::string& { return r.path(); }, op, value);
    if (field == "userAgent") {
        return text(field, [](const LogRecord& r) -> const std::string& { return r.userAgent(); }, op, value);
    }
    throw ComparisonError("field", "unknown field '" + field +
                                       "' (use timestamp, clientIp, method, path, status, bytes or userAgent).");
}

}  // namespace logflow::predicates
