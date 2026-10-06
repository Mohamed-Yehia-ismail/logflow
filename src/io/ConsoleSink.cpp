#include "logflow/io/ConsoleSink.hpp"

#include "logflow/util/Timestamp.hpp"

namespace logflow {

void ConsoleSink::consume(const LogRecord& record) {
    out_ << formatIso8601(record.timestamp()) << "  " << record.clientIp() << "  "
         << record.method() << ' ' << record.path() << "  " << record.status() << "  "
         << record.bytes() << " B";
    if (!record.userAgent().empty()) out_ << "  \"" << record.userAgent() << '"';
    out_ << '\n';
}

}  // namespace logflow
