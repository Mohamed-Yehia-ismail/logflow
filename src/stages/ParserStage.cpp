#include "logflow/stages/ParserStage.hpp"

#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

#include "logflow/util/Timestamp.hpp"

namespace logflow {
namespace {

std::string trim(const std::string& s) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && isSpace(static_cast<unsigned char>(s[begin]))) ++begin;
    while (end > begin && isSpace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(begin, end - begin);
}

bool allDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool allUpper(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < 'A' || c > 'Z') return false;
    }
    return true;
}

// host ident authuser [time] "request" status bytes ["referer" "user-agent"]
// Quoted fields may contain escaped quotes (\").
const std::regex& lineFormat() {
    static const std::regex format(
        R"re(^(\S+)\s+(\S+)\s+(\S+)\s+\[([^\]]*)\]\s+"((?:[^"\\]|\\.)*)"\s+(\S+)\s+(\S+))re"
        R"re((?:\s+"((?:[^"\\]|\\.)*)"\s+"((?:[^"\\]|\\.)*)")?$)re");
    return format;
}

}  // namespace

ParserStage::ParserStage(std::ostream& report) : report_(report) {}

void ParserStage::process(const std::string& line, Emitter<LogRecord>& out) {
    std::optional<LogRecord> record = parse(line);
    if (!record) {
        ++malformed_;
        return;
    }
    ++parsed_;
    out.emit(*record);
}

void ParserStage::close() {
    report_ << "Malformed lines skipped: " << malformed_ << '\n';
}

std::optional<LogRecord> ParserStage::parse(const std::string& line) {
    const std::string text = trim(line);
    std::smatch m;
    if (text.empty() || !std::regex_match(text, m, lineFormat())) return std::nullopt;

    const std::optional<Timestamp> timestamp = parseClfTimestamp(m[4].str());
    if (!timestamp) return std::nullopt;

    // "METHOD path PROTOCOL"
    std::istringstream request(m[5].str());
    std::vector<std::string> parts;
    for (std::string part; request >> part;) parts.push_back(part);
    if (parts.size() != 3 || !allUpper(parts[0]) || parts[2].rfind("HTTP/", 0) != 0) {
        return std::nullopt;
    }

    const std::string status = m[6].str();
    if (status.size() != 3 || !allDigits(status)) return std::nullopt;
    const int statusCode = std::stoi(status);
    if (statusCode < 100 || statusCode > 599) return std::nullopt;

    const std::string bytes = m[7].str();
    long long byteCount = 0;
    if (bytes != "-") {
        if (!allDigits(bytes) || bytes.size() > 18) return std::nullopt;
        byteCount = std::stoll(bytes);
    }

    LogRecord::Builder builder;
    builder.timestamp(*timestamp)
        .clientIp(m[1].str())
        .method(parts[0])
        .path(parts[1])
        .status(statusCode)
        .bytes(byteCount)
        .userAgent(m[9].matched && m[9].str() != "-" ? m[9].str() : "")
        .raw(line);
    if (m[8].matched && m[8].str() != "-" && !m[8].str().empty()) {
        builder.attribute("referer", m[8].str());
    }
    return builder.build();
}

}  // namespace logflow
