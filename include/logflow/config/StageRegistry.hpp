#pragma once

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "logflow/config/ItemType.hpp"
#include "logflow/config/Params.hpp"
#include "logflow/core/Pipeline.hpp"

namespace logflow {

// Creates one component of type S from its configured parameters. A factory validates
// the parameters and reports problems through Params (as ConfigError).
template <typename S>
using StageFactory = std::function<std::unique_ptr<S>(const Params&)>;

// Maps component type names, as written in a configuration file, to factories.
//
// There are three tables: sources, stages and sinks. Registration is data, not a
// switch statement: adding a component type means adding one entry (see
// BuiltinComponents.cpp); no loader code changes. Each entry also records the item
// types the component consumes and produces, so the loader can check that a
// configured pipeline fits together before it builds anything.
class StageRegistry {
public:
    struct SourceEntry {
        ItemType output;
        std::function<Pipeline(const Params&)> create;
    };
    struct StageEntry {
        ItemType input;
        ItemType output;
        std::function<void(Pipeline&, const Params&)> addTo;
    };
    struct SinkEntry {
        ItemType input;
        std::function<void(Pipeline&, const Params&)> setOn;
    };

    template <typename S>
    void addSource(const std::string& name, StageFactory<S> factory) {
        if (sources_.count(name)) throw std::logic_error("source type '" + name + "' registered twice");
        sources_.emplace(name, SourceEntry{ItemType::of<typename S::OutputType>(),
                                           [factory](const Params& p) { return Pipeline(factory(p)); }});
    }

    template <typename S>
    void addStage(const std::string& name, StageFactory<S> factory) {
        if (stages_.count(name)) throw std::logic_error("stage type '" + name + "' registered twice");
        stages_.emplace(name, StageEntry{ItemType::of<typename S::InputType>(),
                                         ItemType::of<typename S::OutputType>(),
                                         [factory](Pipeline& pipeline, const Params& p) {
                                             pipeline.addStage(factory(p));
                                         }});
    }

    // A sink name may be registered once per input type (for example "file" for
    // parsed records and for projected records).
    template <typename S>
    void addSink(const std::string& name, StageFactory<S> factory) {
        const ItemType input = ItemType::of<typename S::InputType>();
        for (const SinkEntry& entry : sinks_[name]) {
            if (entry.input == input) throw std::logic_error("sink type '" + name + "' registered twice");
        }
        sinks_[name].push_back(SinkEntry{input, [factory](Pipeline& pipeline, const Params& p) {
                                             pipeline.setSink(factory(p));
                                         }});
    }

    const SourceEntry* findSource(const std::string& name) const { return find(sources_, name); }
    const StageEntry* findStage(const std::string& name) const { return find(stages_, name); }
    const std::vector<SinkEntry>* findSinks(const std::string& name) const { return find(sinks_, name); }

    std::vector<std::string> sourceNames() const { return names(sources_); }
    std::vector<std::string> stageNames() const { return names(stages_); }
    std::vector<std::string> sinkNames() const { return names(sinks_); }

    // Names of stages that turn `from` into `to`, used to suggest a missing stage.
    std::vector<std::string> stagesConverting(const ItemType& from, const ItemType& to) const {
        std::vector<std::string> result;
        for (const auto& entry : stages_) {
            if (entry.second.input == from && entry.second.output == to) result.push_back(entry.first);
        }
        return result;
    }

private:
    template <typename Map>
    static const typename Map::mapped_type* find(const Map& map, const std::string& name) {
        const auto it = map.find(name);
        return it == map.end() ? nullptr : &it->second;
    }

    template <typename Map>
    static std::vector<std::string> names(const Map& map) {
        std::vector<std::string> result;
        for (const auto& entry : map) result.push_back(entry.first);
        return result;
    }

    std::map<std::string, SourceEntry> sources_;
    std::map<std::string, StageEntry> stages_;
    std::map<std::string, std::vector<SinkEntry>> sinks_;
};

}  // namespace logflow
