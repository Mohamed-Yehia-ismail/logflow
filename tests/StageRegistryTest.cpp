#include "logflow/config/StageRegistry.hpp"

#include <gtest/gtest.h>

#include "logflow/config/BuiltinComponents.hpp"
#include "logflow/config/ConfigError.hpp"
#include "logflow/config/PipelineLoader.hpp"

namespace logflow {
namespace {

// A stage the loader has never heard of: it upper-cases the method.
class ShoutStage : public Stage<LogRecord, LogRecord> {
public:
    void process(const LogRecord& r, Emitter<LogRecord>& out) override {
        out.emit(r.withAttribute("shout", r.method() + "!"));
    }
};

TEST(StageRegistryTest, BuiltinComponentsAreRegistered) {
    const StageRegistry registry = builtinRegistry();
    EXPECT_EQ(registry.sourceNames(), (std::vector<std::string>{"file"}));
    EXPECT_EQ(registry.stageNames(), (std::vector<std::string>{"filter", "parse", "project", "validate"}));
    EXPECT_EQ(registry.sinkNames(), (std::vector<std::string>{"console", "file"}));
    EXPECT_EQ(registry.findSinks("file")->size(), 3u);  // for parsed, projected and failed records
    EXPECT_EQ(registry.findStage("nope"), nullptr);
}

TEST(StageRegistryTest, RecordsItemTypes) {
    const StageRegistry registry = builtinRegistry();
    EXPECT_EQ(registry.findSource("file")->output, ItemType::of<std::string>());
    EXPECT_EQ(registry.findStage("parse")->input, ItemType::of<std::string>());
    EXPECT_EQ(registry.findStage("parse")->output, ItemType::of<LogRecord>());
    EXPECT_EQ(registry.findStage("project")->output, ItemType::of<ProjectedRecord>());
    EXPECT_EQ(registry.stagesConverting(ItemType::of<std::string>(), ItemType::of<LogRecord>()),
              (std::vector<std::string>{"parse"}));
}

TEST(StageRegistryTest, RegisteringTheSameNameTwiceIsAProgrammingError) {
    StageRegistry registry = builtinRegistry();
    EXPECT_THROW(registry.addStage<ShoutStage>("parse", [](const Params&) { return std::make_unique<ShoutStage>(); }),
                 std::logic_error);
}

// The extensibility promise: a new stage type needs one registration and nothing else.
TEST(StageRegistryTest, NewStageTypeIsUsableFromConfigurationWithoutChangingTheLoader) {
    StageRegistry registry = builtinRegistry();
    registry.addStage<ShoutStage>("shout", [](const Params& p) {
        p.allowOnly({});
        return std::make_unique<ShoutStage>();
    });

    LoadedPipeline loaded = PipelineLoader(registry).loadString(
        "pipeline:\n"
        "  source: { type: file, params: { path: " + std::string(LOGFLOW_FIXTURES_DIR) + "/mini-access.log } }\n"
        "  stages:\n"
        "    - type: parse\n"
        "    - type: shout\n"
        "  sink: { type: console }\n");
    EXPECT_EQ(loaded.name, "unnamed");
}

// Passes text lines through unchanged.
class NoopStage : public Stage<std::string, std::string> {
public:
    void process(const std::string& line, Emitter<std::string>& out) override { out.emit(line); }
};

TEST(StageRegistryTest, SinkThatCannotAcceptTheItemTypeIsRejected) {
    StageRegistry registry = builtinRegistry();
    registry.addStage<NoopStage>("noop", [](const Params&) { return std::make_unique<NoopStage>(); });
    try {
        PipelineLoader(registry).loadString(
            "pipeline:\n"
            "  source: { type: file, params: { path: " + std::string(LOGFLOW_FIXTURES_DIR) + "/mini-access.log } }\n"
            "  stages: [ { type: noop } ]\n"
            "  sink: { type: console }\n",
            "test.yaml");
        FAIL() << "expected a ConfigError";
    } catch (const ConfigError& e) {
        EXPECT_STREQ(e.what(), "test.yaml:4: sink (console): this sink cannot write text lines.");
    }
}

}  // namespace
}  // namespace logflow
