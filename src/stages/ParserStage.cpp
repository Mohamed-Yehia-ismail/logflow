#include "logflow/stages/ParserStage.hpp"

#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <vector>

#include "logflow/core/StageException.hpp"
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

// The position of the first byte that is not part of valid UTF-8, or npos.
std::size_t firstInvalidUtf8(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t length = 0;
        unsigned char min = 0x80;  // smallest valid second byte (excludes overlong forms)
        unsigned char max = 0xBF;
        if (c < 0x80) {
            length = 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            length = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            length = 3;
            if (c == 0xE0) min = 0xA0;
            if (c == 0xED) max = 0x9F;  // no UTF-16 surrogates
        } else if (c >= 0xF0 && c <= 0xF4) {
            length = 4;
            if (c == 0xF0) min = 0x90;
            if (c == 0xF4) max = 0x8F;  // nothing above U+10FFFF
        } else {
            return i;
        }
        if (i + length > s.size()) return i;
        for (std::size_t k = 1; k < length; ++k) {
            const unsigned char next = static_cast<unsigned char>(s[i + k]);
            const unsigned char low = k == 1 ? min : 0x80;
            const unsigned char high = k == 1 ? max : 0xBF;
            if (next < low || next > high) return i;
        }
        i += length;
    }
    return std::string::npos;
}

std::string hexByte(char c) {
    static const char digits[] = "0123456789ABCDEF";
    const auto b = static_cast<unsigned char>(c);
    return std::string("0x") + digits[b >> 4] + digits[b & 0xF];
}

[[noreturn]] void reject(const std::string& reason) { throw StageException(reason); }

}  // namespace

ParserStage::ParserStage(std::size_t maxLineLength) : maxLineLength_(maxLineLength) {}

void ParserStage::process(const std::string& line, Emitter<LogRecord>& out) {
    const std::string text = trim(line);
    if (!text.empty() && text[0] == '#') return;  // a comment, not a record
    LogRecord record = parse(line);
    ++parsed_;
    out.emit(record);
}

LogRecord ParserStage::parse(const std::string& line) const {
    // Cheap checks first: the regular expression is never run on an oversized line.
    if (line.size() > maxLineLength_) {
        reject("line is " + std::to_string(line.size()) + " bytes long, the limit is " +
               std::to_string(maxLineLength_));
    }
    const std::size_t badByte = firstInvalidUtf8(line);
    if (badByte != std::string::npos) {
        reject("line is not valid UTF-8 (byte " + hexByte(line[badByte]) + " at position " +
               std::to_string(badByte + 1) + ")");
    }
    const std::string text = trim(line);
    if (text.empty()) reject("empty line");

    std::smatch m;
    if (!std::regex_match(text, m, lineFormat())) {
        reject("line does not have the access-log layout (truncated or missing fields)");
    }

    const std::optional<Timestamp> timestamp = parseClfTimestamp(m[4].str());
    if (!timestamp) reject("invalid timestamp '" + m[4].str() + "'");

    // "METHOD path PROTOCOL"
    std::istringstream request(m[5].str());
    std::vector<std::string> parts;
    for (std::string part; request >> part;) parts.push_back(part);
    if (parts.size() != 3 || !allUpper(parts[0]) || parts[2].rfind("HTTP/", 0) != 0) {
        reject("invalid request '" + m[5].str() + "'");
    }

    const std::string status = m[6].str();
    if (status.size() != 3 || !allDigits(status)) reject("invalid status '" + status + "'");
    const int statusCode = std::stoi(status);
    if (statusCode < 100 || statusCode > 599) reject("invalid status '" + status + "'");

    const std::string bytes = m[7].str();
    long long byteCount = 0;
    if (bytes != "-") {
        const std::string digits = bytes[0] == '-' ? bytes.substr(1) : bytes;
        if (!allDigits(digits) || digits.size() > 18) reject("invalid byte count '" + bytes + "'");
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
