#include "logflow/io/LineFormat.hpp"

#include "logflow/util/Timestamp.hpp"

namespace logflow {
namespace {

std::string quoteIfNeeded(const std::string& value) {
    if (!value.empty() && value.find_first_of(" \t\"=") == std::string::npos) return value;
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') quoted += '\\';
        quoted += c;
    }
    return quoted + '"';
}

// Quotes a value and escapes quotes, backslashes and bytes outside printable ASCII.
std::string quoteEscaped(const std::string& value) {
    static const char digits[] = "0123456789ABCDEF";
    std::string quoted = "\"";
    for (char c : value) {
        const auto b = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            quoted += '\\';
            quoted += c;
        } else if (b < 0x20 || b > 0x7E) {
            quoted += "\\x";
            quoted += digits[b >> 4];
            quoted += digits[b & 0xF];
        } else {
            quoted += c;
        }
    }
    return quoted + '"';
}

}  // namespace

std::string formatLine(const LogRecord& record) {
    std::string line = formatIso8601(record.timestamp()) + "  " + record.clientIp() + "  " +
                       record.method() + ' ' + record.path() + "  " +
                       std::to_string(record.status()) + "  " + std::to_string(record.bytes()) +
                       " B";
    if (!record.userAgent().empty()) line += "  \"" + record.userAgent() + '"';
    return line;
}

std::string formatLine(const ProjectedRecord& record) {
    std::string line;
    for (const ProjectedRecord::Field& field : record.fields()) {
        if (!line.empty()) line += ' ';
        line += field.first + '=' + quoteIfNeeded(field.second);
    }
    return line;
}

std::string formatLine(const FailedRecord& record) {
    return "failedAt=" + formatIso8601(record.timestamp) + " record=" + std::to_string(record.recordNumber) +
           " stage=" + quoteIfNeeded(record.stageName) + " error=" + quoteEscaped(record.exception) +
           " raw=" + quoteEscaped(record.rawInput);
}

}  // namespace logflow
