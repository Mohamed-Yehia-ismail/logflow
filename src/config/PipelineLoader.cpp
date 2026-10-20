#include "logflow/config/PipelineLoader.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "logflow/config/ConfigError.hpp"

namespace logflow {
namespace {

// One configured component: its type name and its parameters.
struct ComponentSpec {
    std::string type;
    Params params;
};

class Reader {
public:
    explicit Reader(std::string file) : file_(std::move(file)) {}

    [[noreturn]] void fail(const YAML::Node& node, const std::string& message) const {
        throw ConfigError(file_ + ":" + std::to_string(lineOf(node)) + ": " + message);
    }

    static int lineOf(const YAML::Node& node) { return node.Mark().line + 1; }

    // Checks that `node` is a mapping that contains only `allowed` keys.
    void expectKeys(const YAML::Node& node, const std::string& what, const std::vector<std::string>& allowed) const {
        if (!node.IsMap()) fail(node, what + " must be a mapping of keys and values.");
        for (const auto& entry : node) {
            const std::string key = entry.first.as<std::string>();
            if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
                fail(entry.first, "unknown key '" + key + "' in " + what + " (allowed: " + join(allowed) + ").");
            }
        }
    }

    YAML::Node required(const YAML::Node& node, const std::string& key, const std::string& what) const {
        const YAML::Node child = node[key];
        if (!child) fail(node, what + " is missing the required key '" + key + "'.");
        return child;
    }

    ComponentSpec component(const YAML::Node& node, const std::string& label) const {
        expectKeys(node, label, {"type", "params"});
        const YAML::Node type = required(node, "type", label);
        if (!type.IsScalar() || type.Scalar().empty()) fail(type, label + ": 'type' must be a name.");

        const std::string description = label + " (" + type.Scalar() + ")";
        std::map<std::string, ParamValue> values;
        if (const YAML::Node params = node["params"]) {
            if (!params.IsMap() && !params.IsNull()) fail(params, description + ": 'params' must be a mapping.");
            for (const auto& entry : params) values.emplace(entry.first.as<std::string>(), value(entry.second, description));
        }
        return ComponentSpec{type.Scalar(), Params(file_, lineOf(node), description, std::move(values))};
    }

    static std::string join(const std::vector<std::string>& names) {
        std::string result;
        for (const std::string& name : names) result += (result.empty() ? "" : ", ") + name;
        return result;
    }

private:
    ParamValue value(const YAML::Node& node, const std::string& description) const {
        ParamValue result;
        result.line = lineOf(node);
        if (node.IsScalar()) {
            result.scalar = node.Scalar();
        } else if (node.IsSequence()) {
            result.list.emplace();
            for (const YAML::Node& item : node) {
                if (!item.IsScalar()) fail(item, description + ": list items must be single values.");
                result.list->push_back(item.Scalar());
            }
        } else {
            fail(node, description + ": a parameter must be a single value or a list.");
        }
        return result;
    }

    std::string file_;
};

// Levenshtein distance, used to suggest the type name the user probably meant.
std::size_t distance(const std::string& a, const std::string& b) {
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) row[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

std::string unknownType(const std::string& kind, const std::string& name, const std::vector<std::string>& known) {
    std::string message = "unknown " + kind + " type '" + name + "'. Known " + kind + " types: " + Reader::join(known) + ".";
    for (const std::string& candidate : known) {
        if (distance(name, candidate) <= 2) return message + " Did you mean '" + candidate + "'?";
    }
    return message;
}

}  // namespace

LoadedPipeline PipelineLoader::loadFile(const std::string& path) const {
    std::ifstream in(path);
    if (!in) throw ConfigError(path + ": cannot open the configuration file.");
    std::stringstream content;
    content << in.rdbuf();
    return loadString(content.str(), path);
}

LoadedPipeline PipelineLoader::loadString(const std::string& yaml, const std::string& sourceName) const {
    const Reader reader(sourceName);

    // 1. Read and check the schema.
    YAML::Node root;
    try {
        root = YAML::Load(yaml);
    } catch (const YAML::ParserException& e) {
        throw ConfigError(sourceName + ":" + std::to_string(e.mark.line + 1) + ": invalid YAML: " + e.msg + ".");
    }
    if (!root || root.IsNull()) throw ConfigError(sourceName + ": the configuration is empty.");
    reader.expectKeys(root, "the configuration", {"pipeline"});
    const YAML::Node pipeline = reader.required(root, "pipeline", "the configuration");
    reader.expectKeys(pipeline, "'pipeline'", {"name", "source", "stages", "sink"});

    const YAML::Node nameNode = pipeline["name"];
    const std::string name = nameNode && nameNode.IsScalar() ? nameNode.Scalar() : "unnamed";

    const ComponentSpec source = reader.component(reader.required(pipeline, "source", "'pipeline'"), "source");
    const YAML::Node stagesNode = reader.required(pipeline, "stages", "'pipeline'");
    if (!stagesNode.IsSequence() && !stagesNode.IsNull()) reader.fail(stagesNode, "'stages' must be a list.");
    if (stagesNode.size() == 0) {
        reader.fail(stagesNode, "pipeline '" + name + "' has no stages. Add at least a 'parse' stage.");
    }
    std::vector<ComponentSpec> stages;
    for (std::size_t i = 0; i < stagesNode.size(); ++i) {
        stages.push_back(reader.component(stagesNode[i], "stage " + std::to_string(i + 1)));
    }
    const ComponentSpec sink = reader.component(reader.required(pipeline, "sink", "'pipeline'"), "sink");

    // 2. Resolve every type name and check that the item types fit together.
    const StageRegistry::SourceEntry* sourceEntry = registry_.findSource(source.type);
    if (!sourceEntry) source.params.fail("", unknownType("source", source.type, registry_.sourceNames()));

    ItemType current = sourceEntry->output;
    std::string previous = "the source";
    std::vector<const StageRegistry::StageEntry*> stageEntries;
    for (const ComponentSpec& stage : stages) {
        const StageRegistry::StageEntry* entry = registry_.findStage(stage.type);
        if (!entry) stage.params.fail("", unknownType("stage", stage.type, registry_.stageNames()));
        if (entry->input != current) {
            std::string message = "this stage needs " + std::string(entry->input.name) + ", but " + previous +
                                  " produces " + current.name + ".";
            const auto fixes = registry_.stagesConverting(current, entry->input);
            if (!fixes.empty()) message += " Add a '" + fixes.front() + "' stage before it.";
            stage.params.fail("", message);
        }
        stageEntries.push_back(entry);
        current = entry->output;
        previous = "the previous stage";
    }

    const std::vector<StageRegistry::SinkEntry>* sinkVariants = registry_.findSinks(sink.type);
    if (!sinkVariants) sink.params.fail("", unknownType("sink", sink.type, registry_.sinkNames()));
    const auto sinkEntry = std::find_if(sinkVariants->begin(), sinkVariants->end(),
                                        [&](const StageRegistry::SinkEntry& e) { return e.input == current; });
    if (sinkEntry == sinkVariants->end()) {
        sink.params.fail("", "this sink cannot write " + std::string(current.name) + ".");
    }

    // 3. Build: each factory validates its parameters and creates its component.
    Pipeline built = sourceEntry->create(source.params);
    for (std::size_t i = 0; i < stages.size(); ++i) stageEntries[i]->addTo(built, stages[i].params);
    sinkEntry->setOn(built, sink.params);
    return LoadedPipeline{name, std::move(built)};
}

}  // namespace logflow
