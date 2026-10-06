#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace logflow {

// A point in time, in UTC.
using Timestamp = std::chrono::system_clock::time_point;

// Parses a Common Log Format timestamp such as "10/Oct/2000:13:55:36 -0700"
// and converts it to UTC. Returns std::nullopt if the text is not a valid date.
std::optional<Timestamp> parseClfTimestamp(const std::string& text);

// Formats a timestamp as ISO 8601 in UTC, for example "2000-10-10T20:55:36Z".
std::string formatIso8601(Timestamp timestamp);

}  // namespace logflow
