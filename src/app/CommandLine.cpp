#include "logflow/app/CommandLine.hpp"

#include <algorithm>
#include <sstream>

#include "logflow/model/LogRecordFields.hpp"

namespace logflow {
namespace {

int parseInt(const std::string& option, const std::string& text) {
    std::size_t used = 0;
    int value = 0;
    try {
        value = std::stoi(text, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    if (used == 0 || used != text.size()) {
        throw UsageError(option + " expects a whole number, got '" + text + "'");
    }
    return value;
}

Timestamp parseTime(const std::string& option, const std::string& text) {
    const std::optional<Timestamp> time = parseIso8601(text);
    if (!time) {
        throw UsageError(option + " expects a UTC time like 2026-09-01T09:00:00Z, got '" + text + "'");
    }
    return *time;
}

std::vector<std::string> parseFields(const std::string& option, const std::string& text) {
    const std::vector<std::string>& known = logRecordFieldNames();
    std::vector<std::string> items;
    std::istringstream in(text);
    for (std::string item; std::getline(in, item, ',');) {
        if (item.empty()) throw UsageError(option + " has an empty field name in '" + text + "'");
        if (std::find(known.begin(), known.end(), item) == known.end()) {
            throw UsageError(option + ": unknown field '" + item + "'");
        }
        items.push_back(item);
    }
    if (items.empty()) throw UsageError(option + " needs at least one field");
    return items;
}

}  // namespace

Options parseCommandLine(const std::vector<std::string>& args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return options;
        }
        if (arg.rfind("--", 0) != 0) {
            if (!options.input.empty()) throw UsageError("more than one input file given");
            options.input = arg;
            continue;
        }
        if (i + 1 >= args.size()) throw UsageError(arg + " needs a value");
        const std::string& value = args[++i];

        if (arg == "--out") options.output = value;
        else if (arg == "--min-status") options.minStatus = parseInt(arg, value);
        else if (arg == "--path-prefix") options.pathPrefix = value;
        else if (arg == "--since") options.since = parseTime(arg, value);
        else if (arg == "--until") options.until = parseTime(arg, value);
        else if (arg == "--fields") options.fields = parseFields(arg, value);
        else throw UsageError("unknown option " + arg);
    }
    if (options.input.empty()) throw UsageError("no input file given");
    return options;
}

const char* usageText() {
    return "Usage: logflow <log-file> [options]\n"
           "\n"
           "Options:\n"
           "  --min-status N        keep records with status >= N\n"
           "  --path-prefix P       keep records whose path starts with P\n"
           "  --since TIME          keep records at or after TIME (e.g. 2026-09-01T09:00:00Z)\n"
           "  --until TIME          keep records before TIME\n"
           "  --fields a,b,c        output only these fields: timestamp, clientIp, method,\n"
           "                        path, status, bytes, userAgent\n"
           "  --out FILE            write to FILE instead of the console\n"
           "  --help                show this help\n";
}

}  // namespace logflow
