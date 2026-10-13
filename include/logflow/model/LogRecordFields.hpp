#pragma once

#include <optional>
#include <string>
#include <vector>

#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Access to LogRecord fields by name, for components that are configured with
// field names (projection, command-line options).
//
// Field names: timestamp, clientIp, method, path, status, bytes, userAgent.
// raw is deliberately not selectable: it would bring back everything a
// projection is meant to remove.

// All selectable field names, in record order.
const std::vector<std::string>& logRecordFieldNames();

// The field rendered as text (timestamp as ISO 8601), or std::nullopt if there is
// no field with that name.
std::optional<std::string> logRecordField(const LogRecord& record, const std::string& name);

}  // namespace logflow
