// The registration table. To add a component type, add one entry here: the
// loader finds it by name, and no other code changes.

#include "logflow/config/BuiltinComponents.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "logflow/io/ConsoleSink.hpp"
#include "logflow/io/FileLineSource.hpp"
#include "logflow/io/FileSink.hpp"
#include "logflow/model/LogRecordFields.hpp"
#include "logflow/stages/ParserStage.hpp"
#include "logflow/stages/PredicateFilterStage.hpp"
#include "logflow/stages/Predicates.hpp"
#include "logflow/stages/ProjectionStage.hpp"
#include "logflow/stages/ValidationStage.hpp"
#include "logflow/util/Timestamp.hpp"

namespace logflow {
namespace {

// --- sources ---

std::unique_ptr<FileLineSource> fileSource(const Params& p) {
    p.allowOnly({"path"});
    const std::string path = p.requireString("path");
    if (!std::filesystem::is_regular_file(path)) p.fail("path", "input file '" + path + "' does not exist.");
    return std::make_unique<FileLineSource>(path);
}

// --- stages ---

std::unique_ptr<ParserStage> parseStage(const Params& p) {
    p.allowOnly({"format", "maxLineLength"});
    const std::string format = p.optionalString("format").value_or("clf");
    if (format != "clf" && format != "combined") {
        p.fail("format", "unknown log format '" + format + "' (use clf or combined).");
    }
    const long long maxLength = p.optionalInteger("maxLineLength", 64, 1024 * 1024)
                                    .value_or(static_cast<long long>(ParserStage::kDefaultMaxLineLength));
    return std::make_unique<ParserStage>(static_cast<std::size_t>(maxLength));
}

std::unique_ptr<PredicateFilterStage<>> filterStage(const Params& p) {
    p.allowOnly({"field", "op", "value"});
    const std::string field = p.requireString("field");
    const std::string op = p.requireString("op");
    const std::string value = p.requireString("value");
    try {
        return std::make_unique<PredicateFilterStage<>>(predicates::compare(field, op, value));
    } catch (const predicates::ComparisonError& e) {
        p.fail(e.part, e.what());
    }
}

std::unique_ptr<ProjectionStage> projectStage(const Params& p) {
    p.allowOnly({"fields"});
    const std::vector<std::string> fields = p.requireList("fields");
    const std::vector<std::string>& known = logRecordFieldNames();
    for (const std::string& field : fields) {
        if (std::find(known.begin(), known.end(), field) == known.end()) {
            p.fail("fields", "unknown field '" + field +
                                 "' (use timestamp, clientIp, method, path, status, bytes or userAgent).");
        }
    }
    return std::make_unique<ProjectionStage>(fields);
}

// Every parameter except `required` is a rule on the field of the same name:
//   required: [clientIp, method, path]
//   status: 100..599            bytes: 0..
//   timestamp: 2000-01-01T00:00:00Z..2030-12-31T23:59:59Z
//   clientIp: ipv4              (or ipv6, ip)
std::unique_ptr<ValidationStage> validateStage(const Params& p) {
    p.allowOnly({"required", "status", "bytes", "timestamp", "clientIp"});
    std::vector<ValidationRule> rules;

    const auto range = [&p](const std::string& key, const std::string& example) {
        const std::string text = *p.optionalString(key);
        const auto bounds = validation::splitRange(text);
        if (!bounds || (bounds->first.empty() && bounds->second.empty())) {
            p.fail(key, "'" + text + "' is not a range. Write min..max, for example " + example + ".");
        }
        return *bounds;
    };
    const auto number = [&p](const std::string& key, const std::string& text) -> std::optional<long long> {
        if (text.empty()) return std::nullopt;
        const std::string digits = text[0] == '-' ? text.substr(1) : text;
        if (digits.empty() || digits.size() > 18 || digits.find_first_not_of("0123456789") != std::string::npos) {
            p.fail(key, "'" + text + "' is not a whole number.");
        }
        return std::stoll(text);
    };
    const auto time = [&p](const std::string& text) -> std::optional<Timestamp> {
        if (text.empty()) return std::nullopt;
        const std::optional<Timestamp> parsed = parseIso8601(text);
        if (!parsed) p.fail("timestamp", "'" + text + "' is not a time like 2026-01-01T00:00:00Z.");
        return parsed;
    };

    if (p.has("required")) {
        for (const std::string& field : p.requireList("required")) {
            try {
                rules.push_back(validation::required(field));
            } catch (const std::invalid_argument& e) {
                p.fail("required", e.what() + std::string("."));
            }
        }
    }
    for (const std::string key : {"status", "bytes"}) {
        if (!p.has(key)) continue;
        const auto bounds = range(key, key == "status" ? "100..599" : "0..");
        const auto min = number(key, bounds.first);
        const auto max = number(key, bounds.second);
        if (min && max && *min > *max) p.fail(key, "the minimum is larger than the maximum.");
        rules.push_back(validation::range(key, min, max));
    }
    if (p.has("timestamp")) {
        const auto bounds = range("timestamp", "2000-01-01T00:00:00Z..2030-12-31T23:59:59Z");
        const auto from = time(bounds.first);
        const auto to = time(bounds.second);
        if (from && to && *from > *to) p.fail("timestamp", "the start is later than the end.");
        rules.push_back(validation::timestampRange(from, to));
    }
    if (const std::optional<std::string> format = p.optionalString("clientIp")) {
        try {
            rules.push_back(validation::ipFormat(*format));
        } catch (const std::invalid_argument& e) {
            p.fail("clientIp", e.what() + std::string("."));
        }
    }

    if (rules.empty()) p.fail("", "no validation rules. Add at least one, for example status: 100..599.");
    return std::make_unique<ValidationStage>(std::move(rules));
}

// --- sinks ---

template <typename T>
std::unique_ptr<FileSink<T>> fileSink(const Params& p) {
    p.allowOnly({"path"});
    return std::make_unique<FileSink<T>>(p.requireString("path"));
}

template <typename T>
std::unique_ptr<ConsoleSink<T>> consoleSink(const Params& p) {
    p.allowOnly({});
    return std::make_unique<ConsoleSink<T>>();
}

}  // namespace

void registerBuiltinComponents(StageRegistry& registry) {
    registry.addSource<FileLineSource>("file", fileSource);

    registry.addStage<ParserStage>("parse", parseStage);
    registry.addStage<PredicateFilterStage<>>("filter", filterStage);
    registry.addStage<ProjectionStage>("project", projectStage);
    registry.addStage<ValidationStage>("validate", validateStage);

    registry.addSink<FileSink<LogRecord>>("file", fileSink<LogRecord>);
    registry.addSink<FileSink<ProjectedRecord>>("file", fileSink<ProjectedRecord>);
    registry.addSink<ConsoleSink<LogRecord>>("console", consoleSink<LogRecord>);
    registry.addSink<ConsoleSink<ProjectedRecord>>("console", consoleSink<ProjectedRecord>);
    registry.addSink<FileSink<FailedRecord>>("file", fileSink<FailedRecord>);
    registry.addSink<ConsoleSink<FailedRecord>>("console", consoleSink<FailedRecord>);
}

StageRegistry builtinRegistry() {
    StageRegistry registry;
    registerBuiltinComponents(registry);
    return registry;
}

}  // namespace logflow
