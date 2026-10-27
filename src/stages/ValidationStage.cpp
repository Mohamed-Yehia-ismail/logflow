#include "logflow/stages/ValidationStage.hpp"

#include <cctype>
#include <stdexcept>

#include "logflow/core/StageException.hpp"

namespace logflow {

ValidationStage::ValidationStage(std::vector<ValidationRule> rules) : rules_(std::move(rules)) {
    if (rules_.empty()) throw std::invalid_argument("ValidationStage needs at least one rule");
}

void ValidationStage::process(const LogRecord& record, Emitter<LogRecord>& out) {
    std::string problems;
    for (const ValidationRule& rule : rules_) {
        if (const std::optional<std::string> problem = rule.check(record)) {
            problems += (problems.empty() ? "" : "; ") + *problem;
        }
    }
    if (!problems.empty()) throw StageException("validation failed: " + problems);
    out.emit(record);
}

namespace validation {
namespace {

bool isIpv4(const std::string& text) {
    int parts = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = text.find('.', start);
        const std::string part = text.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (part.empty() || part.size() > 3) return false;
        for (char c : part) {
            if (c < '0' || c > '9') return false;
        }
        if (part.size() > 1 && part[0] == '0') return false;  // no leading zeros
        if (std::stoi(part) > 255) return false;
        ++parts;
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return parts == 4;
}

bool isIpv6(const std::string& text) {
    if (text.empty() || text.size() > 39) return false;
    const std::size_t gap = text.find("::");
    if (gap != std::string::npos && text.find("::", gap + 1) != std::string::npos) return false;

    int groups = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t colon = text.find(':', start);
        const std::size_t end = colon == std::string::npos ? text.size() : colon;
        const std::string group = text.substr(start, end - start);
        if (group.size() > 4) return false;
        for (char c : group) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }
        if (!group.empty()) ++groups;
        if (colon == std::string::npos) break;
        start = colon + 1;
    }
    return gap == std::string::npos ? groups == 8 : groups < 8;
}

}  // namespace

ValidationRule required(const std::string& field) {
    std::function<const std::string&(const LogRecord&)> get;
    if (field == "clientIp") get = [](const LogRecord& r) -> const std::string& { return r.clientIp(); };
    else if (field == "method") get = [](const LogRecord& r) -> const std::string& { return r.method(); };
    else if (field == "path") get = [](const LogRecord& r) -> const std::string& { return r.path(); };
    else if (field == "userAgent") get = [](const LogRecord& r) -> const std::string& { return r.userAgent(); };
    else throw std::invalid_argument("field '" + field + "' cannot be required (use clientIp, method, path or userAgent)");

    return ValidationRule{field + " is required", [field, get](const LogRecord& r) -> std::optional<std::string> {
                              if (get(r).empty()) return field + " is missing";
                              return std::nullopt;
                          }};
}

ValidationRule range(const std::string& field, std::optional<long long> min, std::optional<long long> max) {
    std::function<long long(const LogRecord&)> get;
    if (field == "status") get = [](const LogRecord& r) { return static_cast<long long>(r.status()); };
    else if (field == "bytes") get = [](const LogRecord& r) { return r.bytes(); };
    else throw std::invalid_argument("field '" + field + "' has no numeric range (use status or bytes)");

    const std::string limits = (min ? std::to_string(*min) : "") + ".." + (max ? std::to_string(*max) : "");
    return ValidationRule{field + " in " + limits, [field, get, min, max, limits](const LogRecord& r)
                                                       -> std::optional<std::string> {
                              const long long value = get(r);
                              if ((min && value < *min) || (max && value > *max)) {
                                  return field + " " + std::to_string(value) + " is outside " + limits;
                              }
                              return std::nullopt;
                          }};
}

ValidationRule timestampRange(std::optional<Timestamp> from, std::optional<Timestamp> to) {
    const std::string limits = (from ? formatIso8601(*from) : "") + ".." + (to ? formatIso8601(*to) : "");
    return ValidationRule{"timestamp in " + limits, [from, to, limits](const LogRecord& r)
                                                        -> std::optional<std::string> {
                              if ((from && r.timestamp() < *from) || (to && r.timestamp() > *to)) {
                                  return "timestamp " + formatIso8601(r.timestamp()) + " is outside " + limits;
                              }
                              return std::nullopt;
                          }};
}

ValidationRule ipFormat(const std::string& format) {
    std::function<bool(const std::string&)> valid;
    std::string label;
    if (format == "ipv4") {
        valid = isIpv4;
        label = "an IPv4 address";
    } else if (format == "ipv6") {
        valid = isIpv6;
        label = "an IPv6 address";
    } else if (format == "ip") {
        valid = [](const std::string& text) { return isIpv4(text) || isIpv6(text); };
        label = "an IP address";
    } else {
        throw std::invalid_argument("unknown address format '" + format + "' (use ipv4, ipv6 or ip)");
    }
    return ValidationRule{"clientIp is " + label, [valid, label](const LogRecord& r) -> std::optional<std::string> {
                              if (!valid(r.clientIp())) return "clientIp '" + r.clientIp() + "' is not " + label;
                              return std::nullopt;
                          }};
}

std::optional<std::pair<std::string, std::string>> splitRange(const std::string& text) {
    const std::size_t dots = text.find("..");
    if (dots == std::string::npos) return std::nullopt;
    return std::make_pair(text.substr(0, dots), text.substr(dots + 2));
}

}  // namespace validation
}  // namespace logflow
