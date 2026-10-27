#include "logflow/config/PipelineLoader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "logflow/config/BuiltinComponents.hpp"
#include "logflow/config/ConfigError.hpp"

namespace logflow {
namespace {

namespace fs = std::filesystem;

const std::string kFixture = std::string(LOGFLOW_FIXTURES_DIR) + "/mini-access.log";

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream content;
    content << in.rdbuf();
    return content.str();
}

class PipelineLoaderTest : public ::testing::Test {
protected:
    void TearDown() override { fs::remove(out); }

    // A configuration with the given stages list, reading the fixture and writing to `out`.
    std::string config(const std::string& stages) const {
        return "pipeline:\n"
               "  name: test\n"
               "  source:\n"
               "    type: file\n"
               "    params: { path: " + kFixture + " }\n"
               "  stages:\n" + stages +
               "  sink:\n"
               "    type: file\n"
               "    params: { path: " + out.string() + " }\n";
    }

    // Loads `yaml` and returns the ConfigError message, or "" if it loaded.
    std::string errorOf(const std::string& yaml) const {
        try {
            loader.loadString(yaml, "test.yaml");
        } catch (const ConfigError& e) {
            return e.what();
        }
        return "";
    }

    const fs::path out = fs::temp_directory_path() / "logflow-loader-test.txt";
    const StageRegistry registry = builtinRegistry();
    const PipelineLoader loader{registry};
};

// 1. Valid configuration
TEST_F(PipelineLoaderTest, ValidConfigurationBuildsAWorkingPipeline) {
    LoadedPipeline loaded = loader.loadString(config(
        "    - type: parse\n"
        "      params: { format: clf }\n"
        "    - type: filter\n"
        "      params: { field: status, op: \">=\", value: 400 }\n"
        "    - type: project\n"
        "      params: { fields: [status, path] }\n"));
    EXPECT_EQ(loaded.name, "test");

    loaded.pipeline.run();
    EXPECT_EQ(readFile(out),
              "status=500 path=/api/v1/users\n"
              "status=403 path=/admin\n"
              "status=404 path=/api/v1/orders/7\n"
              "status=503 path=\"/api/v1/users?page=2\"\n");
}

// 2. Unknown stage type
TEST_F(PipelineLoaderTest, UnknownStageTypeIsRejectedWithASuggestion) {
    const std::string error = errorOf(config("    - type: parse\n    - type: filtr\n"));
    EXPECT_EQ(error,
              "test.yaml:8: stage 2 (filtr): unknown stage type 'filtr'. "
              "Known stage types: filter, parse, project, validate. Did you mean 'filter'?");
}

// 3. Missing parameter
TEST_F(PipelineLoaderTest, MissingParameterIsRejected) {
    const std::string error = errorOf(config(
        "    - type: parse\n"
        "    - type: filter\n"
        "      params: { field: status, op: \">=\" }\n"));
    EXPECT_EQ(error,
              "test.yaml:8: stage 2 (filter): missing required parameter 'value'. "
              "Expected parameters: field, op, value.");
}

// 4. Parameter of the wrong type
TEST_F(PipelineLoaderTest, ParameterOfTheWrongTypeIsRejected) {
    EXPECT_EQ(errorOf(config(
                  "    - type: parse\n"
                  "    - type: filter\n"
                  "      params: { field: status, op: \">=\", value: four-hundred }\n")),
              "test.yaml:9: stage 2 (filter): value 'four-hundred' for field 'status' must be a whole number.");

    EXPECT_EQ(errorOf(config(
                  "    - type: parse\n"
                  "    - type: project\n"
                  "      params: { fields: status }\n")),
              "test.yaml:9: stage 2 (project): parameter 'fields' must be a list, for example fields: [a, b].");

    EXPECT_EQ(errorOf(config(
                  "    - type: parse\n"
                  "    - type: filter\n"
                  "      params: { field: [status], op: \">=\", value: 400 }\n")),
              "test.yaml:9: stage 2 (filter): parameter 'field' must be a single value, not a list.");
}

// 5. Empty pipeline
TEST_F(PipelineLoaderTest, EmptyPipelineIsRejected) {
    EXPECT_EQ(errorOf(config("    []\n")),
              "test.yaml:7: pipeline 'test' has no stages. Add at least a 'parse' stage.");
    EXPECT_EQ(errorOf(""), "test.yaml: the configuration is empty.");
    EXPECT_EQ(errorOf("# only a comment\n"), "test.yaml: the configuration is empty.");
}

// ---- more configuration-time errors ----

TEST_F(PipelineLoaderTest, StagesInAnImpossibleOrderAreRejected) {
    EXPECT_EQ(errorOf(config("    - type: filter\n      params: { field: status, op: \">=\", value: 400 }\n")),
              "test.yaml:7: stage 1 (filter): this stage needs parsed records, but the source produces text "
              "lines. Add a 'parse' stage before it.");
    EXPECT_EQ(errorOf(config("    - type: parse\n    - type: project\n      params: { fields: [status] }\n"
                             "    - type: parse\n")),
              "test.yaml:10: stage 3 (parse): this stage needs text lines, but the previous stage produces "
              "projected records.");
}

TEST_F(PipelineLoaderTest, UnknownSourceSinkAndKeysAreRejected) {
    EXPECT_EQ(errorOf("pipeline:\n  source: { type: kafka }\n  stages: [ { type: parse } ]\n  sink: { type: console }\n"),
              "test.yaml:2: source (kafka): unknown source type 'kafka'. Known source types: file.");
    EXPECT_EQ(errorOf("pipeline:\n  source: { type: file, params: { path: " + kFixture +
                      " } }\n  stages: [ { type: parse } ]\n  sink: { type: printer }\n"),
              "test.yaml:4: sink (printer): unknown sink type 'printer'. Known sink types: console, file.");
    EXPECT_EQ(errorOf("pipeline:\n  stage: []\n"),
              "test.yaml:2: unknown key 'stage' in 'pipeline' (allowed: name, source, stages, sink, deadLetter).");
    EXPECT_EQ(errorOf("pipelines: {}\n"),
              "test.yaml:1: unknown key 'pipelines' in the configuration (allowed: pipeline).");
    EXPECT_EQ(errorOf(config("    - type: parse\n      params: { format: clf, strict: yes }\n")),
              "test.yaml:8: stage 1 (parse): unknown parameter 'strict'. Expected parameters: format, maxLineLength.");
}

TEST_F(PipelineLoaderTest, MissingRequiredKeysAreRejected) {
    EXPECT_EQ(errorOf("pipeline:\n  stages: [ { type: parse } ]\n  sink: { type: console }\n"),
              "test.yaml:2: 'pipeline' is missing the required key 'source'.");
    EXPECT_EQ(errorOf("pipeline:\n  source: { params: { path: x } }\n"),
              "test.yaml:2: source is missing the required key 'type'.");
}

TEST_F(PipelineLoaderTest, InvalidValuesAreRejected) {
    EXPECT_EQ(errorOf(config("    - type: parse\n      params: { format: json }\n")),
              "test.yaml:8: stage 1 (parse): unknown log format 'json' (use clf or combined).");
    EXPECT_EQ(errorOf(config("    - type: parse\n    - type: filter\n"
                             "      params: { field: path, op: \">\", value: /api }\n")),
              "test.yaml:9: stage 2 (filter): operator '>' is not supported for text field 'path' "
              "(use ==, !=, startsWith, endsWith, contains).");
    EXPECT_EQ(errorOf(config("    - type: parse\n    - type: project\n      params: { fields: [status, password] }\n")),
              "test.yaml:9: stage 2 (project): unknown field 'password' (use timestamp, clientIp, method, path, "
              "status, bytes or userAgent).");
    EXPECT_EQ(errorOf("pipeline:\n  source: { type: file, params: { path: /no/such/file.log } }\n"
                      "  stages: [ { type: parse } ]\n  sink: { type: console }\n"),
              "test.yaml:2: source (file): input file '/no/such/file.log' does not exist.");
}

TEST_F(PipelineLoaderTest, ParametersMustBeValuesOrLists) {
    EXPECT_EQ(errorOf(config("    - type: parse\n      params: { format: { name: clf } }\n")),
              "test.yaml:8: stage 1 (parse): a parameter must be a single value or a list.");
    EXPECT_EQ(errorOf(config("    - type: parse\n    - type: project\n      params: { fields: [[status]] }\n")),
              "test.yaml:9: stage 2 (project): list items must be single values.");
}

TEST_F(PipelineLoaderTest, InvalidYamlIsRejected) {
    EXPECT_EQ(errorOf("pipeline:\n  source: { type: file\n"), "test.yaml:3: invalid YAML: end of map flow not found.");
}

TEST_F(PipelineLoaderTest, MissingConfigurationFileIsRejected) {
    EXPECT_THROW(loader.loadFile("/no/such/config.yaml"), ConfigError);
}

TEST_F(PipelineLoaderTest, ErrorsAreFoundBeforeAnyDataIsWritten) {
    // The sink is valid, but a later problem must stop loading before the output file exists.
    errorOf(config("    - type: parse\n    - type: filtr\n"));
    EXPECT_FALSE(fs::exists(out));
}

// ---- error policies, dead-letter sink and validation (Increment 5) ----

TEST_F(PipelineLoaderTest, ErrorPolicyAndDeadLetterSinkAreConfigured) {
    const fs::path deadLetter = fs::temp_directory_path() / "logflow-loader-dead-letter.txt";
    LoadedPipeline loaded = loader.loadString(
        config("    - type: parse\n"
               "      onError: dead_letter\n"
               "    - type: validate\n"
               "      params: { status: 100..599 }\n"
               "      onError: RETRY(2)\n") +
        "  deadLetter:\n"
        "    type: file\n"
        "    params: { path: " + deadLetter.string() + " }\n");
    const RunSummary summary = loaded.pipeline.run();

    EXPECT_EQ(summary.pipelineName, "test");
    EXPECT_EQ(summary.stages[0].name, "parse");
    EXPECT_EQ(summary.stages[0].policy, "DEAD_LETTER");
    EXPECT_EQ(summary.stages[1].policy, "RETRY(2)");
    EXPECT_EQ(summary.stages[0].deadLettered, 1u);  // the malformed fixture line
    EXPECT_NE(readFile(deadLetter).find("stage=parse"), std::string::npos);
    fs::remove(deadLetter);
}

TEST_F(PipelineLoaderTest, DefaultErrorPolicyIsSkip) {
    LoadedPipeline loaded = loader.loadString(config("    - type: parse\n"));
    EXPECT_EQ(loaded.pipeline.run().stages[0].policy, "SKIP");
}

TEST_F(PipelineLoaderTest, UnknownErrorPolicyIsReportedAtItsLine) {
    EXPECT_EQ(errorOf(config("    - type: parse\n"
                             "      onError: IGNORE\n")),
              "test.yaml:8: stage 1 (parse): unknown error policy 'IGNORE' "
              "(use FAIL_FAST, SKIP, DEAD_LETTER or RETRY(n) with n from 1 to 10).");
}

TEST_F(PipelineLoaderTest, DeadLetterPolicyWithoutDeadLetterSinkIsRejected) {
    const std::string message = errorOf(config("    - type: parse\n"
                                               "      onError: DEAD_LETTER\n"));
    EXPECT_EQ(message.rfind("test.yaml:8: stage 1 (parse): onError DEAD_LETTER needs a dead-letter sink", 0), 0u)
        << message;
}

TEST_F(PipelineLoaderTest, DeadLetterSinkMustAcceptFailedRecords) {
    const std::string base = config("    - type: parse\n      onError: DEAD_LETTER\n");
    EXPECT_EQ(errorOf(base + "  deadLetter: { type: database }\n").find("unknown sink type 'database'") ==
                  std::string::npos,
              false);
    EXPECT_EQ(errorOf(base + "  deadLetter: { type: file, params: { path: x.txt, mode: append } }\n")
                  .find("unknown parameter 'mode'") == std::string::npos,
              false);
}

TEST_F(PipelineLoaderTest, OnErrorIsOnlyAllowedOnStages) {
    const std::string yaml = "pipeline:\n"
                             "  source: { type: file, params: { path: " + kFixture + " }, onError: SKIP }\n"
                             "  stages: [ { type: parse } ]\n"
                             "  sink: { type: console }\n";
    EXPECT_NE(errorOf(yaml).find("unknown key 'onError' in source"), std::string::npos) << errorOf(yaml);
    EXPECT_NE(errorOf(config("    - type: parse\n      onError: [SKIP]\n")).find("'onError' must be a policy name"),
              std::string::npos);
}

TEST_F(PipelineLoaderTest, ValidateStageParametersAreChecked) {
    const auto validate = [this](const std::string& params) {
        return errorOf(config("    - type: parse\n    - type: validate\n      params: { " + params + " }\n"));
    };
    EXPECT_EQ(validate("required: [clientIp, path], status: 100..599, bytes: 0.., clientIp: ipv4, "
                       "timestamp: 2000-01-01T00:00:00Z..2030-12-31T23:59:59Z"),
              "");
    EXPECT_NE(validate("status: 100").find("'100' is not a range"), std::string::npos);
    EXPECT_NE(validate("status: ..").find("is not a range"), std::string::npos);
    EXPECT_NE(validate("bytes: a..b").find("'a' is not a whole number"), std::string::npos);
    EXPECT_NE(validate("status: 600..100").find("minimum is larger"), std::string::npos);
    EXPECT_NE(validate("timestamp: yesterday..").find("'yesterday' is not a time"), std::string::npos);
    EXPECT_NE(validate("timestamp: 2030-01-01T00:00:00Z..2000-01-01T00:00:00Z").find("start is later"),
              std::string::npos);
    EXPECT_NE(validate("clientIp: mac").find("unknown address format 'mac'"), std::string::npos);
    EXPECT_NE(validate("required: [status]").find("cannot be required"), std::string::npos);
    EXPECT_NE(validate("color: red").find("unknown parameter 'color'"), std::string::npos);
    EXPECT_NE(errorOf(config("    - type: parse\n    - type: validate\n")).find("no validation rules"),
              std::string::npos);
}

TEST_F(PipelineLoaderTest, MaxLineLengthIsChecked) {
    EXPECT_EQ(errorOf(config("    - type: parse\n      params: { maxLineLength: 4096 }\n")), "");
    EXPECT_NE(errorOf(config("    - type: parse\n      params: { maxLineLength: big }\n")).find("whole number"),
              std::string::npos);
    EXPECT_NE(errorOf(config("    - type: parse\n      params: { maxLineLength: 10 }\n")).find("must be from 64"),
              std::string::npos);
}

}  // namespace
}  // namespace logflow
