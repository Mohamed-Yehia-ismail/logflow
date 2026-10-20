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
    p.allowOnly({"format"});
    const std::string format = p.optionalString("format").value_or("clf");
    if (format != "clf" && format != "combined") {
        p.fail("format", "unknown log format '" + format + "' (use clf or combined).");
    }
    return std::make_unique<ParserStage>();
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

    registry.addSink<FileSink<LogRecord>>("file", fileSink<LogRecord>);
    registry.addSink<FileSink<ProjectedRecord>>("file", fileSink<ProjectedRecord>);
    registry.addSink<ConsoleSink<LogRecord>>("console", consoleSink<LogRecord>);
    registry.addSink<ConsoleSink<ProjectedRecord>>("console", consoleSink<ProjectedRecord>);
}

StageRegistry builtinRegistry() {
    StageRegistry registry;
    registerBuiltinComponents(registry);
    return registry;
}

}  // namespace logflow
