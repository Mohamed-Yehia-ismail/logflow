#include "logflow/model/LogRecord.hpp"

namespace logflow {

std::optional<std::string> LogRecord::attribute(const std::string& key) const {
    const auto it = attributes_.find(key);
    if (it == attributes_.end()) return std::nullopt;
    return it->second;
}

LogRecord LogRecord::withAttribute(const std::string& key, const std::string& value) const {
    LogRecord copy = *this;
    copy.attributes_[key] = value;
    return copy;
}

}  // namespace logflow
