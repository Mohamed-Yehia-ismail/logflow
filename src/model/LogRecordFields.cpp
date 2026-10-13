#include "logflow/model/LogRecordFields.hpp"

#include "logflow/util/Timestamp.hpp"

namespace logflow {

const std::vector<std::string>& logRecordFieldNames() {
    static const std::vector<std::string> names = {
        "timestamp", "clientIp", "method", "path", "status", "bytes", "userAgent"};
    return names;
}

std::optional<std::string> logRecordField(const LogRecord& record, const std::string& name) {
    if (name == "timestamp") return formatIso8601(record.timestamp());
    if (name == "clientIp") return record.clientIp();
    if (name == "method") return record.method();
    if (name == "path") return record.path();
    if (name == "status") return std::to_string(record.status());
    if (name == "bytes") return std::to_string(record.bytes());
    if (name == "userAgent") return record.userAgent();
    return std::nullopt;
}

}  // namespace logflow
