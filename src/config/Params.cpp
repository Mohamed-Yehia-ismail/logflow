#include "logflow/config/Params.hpp"

#include <algorithm>
#include <utility>

#include "logflow/config/ConfigError.hpp"

namespace logflow {

Params::Params(std::string file, int line, std::string component, std::map<std::string, ParamValue> values)
    : file_(std::move(file)), line_(line), component_(std::move(component)), values_(std::move(values)) {}

void Params::allowOnly(const std::vector<std::string>& names) const {
    known_ = names;
    for (const auto& entry : values_) {
        if (std::find(names.begin(), names.end(), entry.first) == names.end()) {
            fail(entry.first, "unknown parameter '" + entry.first + "'." + expectedList());
        }
    }
}

std::string Params::requireString(const std::string& key) const {
    const ParamValue& value = require(key);
    if (!value.scalar) fail(key, "parameter '" + key + "' must be a single value, not a list.");
    return *value.scalar;
}

std::optional<std::string> Params::optionalString(const std::string& key) const {
    if (values_.count(key) == 0) return std::nullopt;
    return requireString(key);
}

std::vector<std::string> Params::requireList(const std::string& key) const {
    const ParamValue& value = require(key);
    if (!value.list) {
        fail(key, "parameter '" + key + "' must be a list, for example " + key + ": [a, b].");
    }
    if (value.list->empty()) fail(key, "parameter '" + key + "' must not be empty.");
    return *value.list;
}

std::optional<long long> Params::optionalInteger(const std::string& key, long long min, long long max) const {
    const std::optional<std::string> text = optionalString(key);
    if (!text) return std::nullopt;
    const std::string range = std::to_string(min) + " to " + std::to_string(max);
    if (text->empty() || text->size() > 18 || text->find_first_not_of("0123456789") != std::string::npos) {
        fail(key, "parameter '" + key + "' must be a whole number from " + range + ".");
    }
    const long long value = std::stoll(*text);
    if (value < min || value > max) fail(key, "parameter '" + key + "' must be from " + range + ".");
    return value;
}

void Params::fail(const std::string& key, const std::string& message) const {
    const auto it = values_.find(key);
    const int line = it != values_.end() ? it->second.line : line_;
    throw ConfigError(file_ + ":" + std::to_string(line) + ": " + component_ + ": " + message);
}

const ParamValue& Params::require(const std::string& key) const {
    const auto it = values_.find(key);
    if (it == values_.end()) fail("", "missing required parameter '" + key + "'." + expectedList());
    return it->second;
}

std::string Params::expectedList() const {
    if (known_.empty()) return "";
    std::string list;
    for (const std::string& name : known_) list += (list.empty() ? "" : ", ") + name;
    return " Expected parameters: " + list + ".";
}

}  // namespace logflow
