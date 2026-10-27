#pragma once

#include <string>

#include "logflow/core/FailedRecord.hpp"
#include "logflow/model/LogRecord.hpp"
#include "logflow/model/ProjectedRecord.hpp"

namespace logflow {

// How records are written as one line of text. Shared by every sink that writes text.

// 2026-09-01T08:01:22Z  192.0.2.38  GET /contact  200  18224 B  "Mozilla/5.0 ..."
std::string formatLine(const LogRecord& record);

// logfmt style: timestamp=2026-09-01T08:01:22Z clientIp=192.0.2.38 path=/contact
// Values that are empty or contain spaces, quotes or '=' are quoted.
std::string formatLine(const ProjectedRecord& record);

// A dead-letter entry, logfmt style:
// failedAt=2026-10-27T15:00:00Z record=4 stage=parse error="..." raw="..."
// In raw, bytes that are not printable ASCII are written as \xHH, so the line stays
// readable text and the original bytes can still be recovered.
std::string formatLine(const FailedRecord& record);

}  // namespace logflow
