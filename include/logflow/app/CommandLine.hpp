#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "logflow/util/Timestamp.hpp"

namespace logflow {

// What the user asked for on the command line.
struct Options {
    std::string input;
    std::optional<std::string> output;               // --out FILE (default: console)
    std::optional<int> minStatus;                    // --min-status N
    std::optional<std::string> pathPrefix;           // --path-prefix P
    std::optional<Timestamp> since;                  // --since ISO-8601 (inclusive)
    std::optional<Timestamp> until;                  // --until ISO-8601 (exclusive)
    std::optional<std::vector<std::string>> fields;  // --fields a,b,c
    bool help = false;                               // --help
};

// Thrown when the command line is not valid.
class UsageError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

// Parses the arguments that follow the program name.
Options parseCommandLine(const std::vector<std::string>& args);

// The text printed for --help and after a usage error.
const char* usageText();

}  // namespace logflow
