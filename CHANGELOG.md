# Changelog

## v5: Error Handling, Validation and the Dead-Letter Channel

Bad records are no longer skipped silently. Each stage has a configured error policy,
failed records can be routed to a separate dead-letter sink, a validation stage checks
records against declarative rules, and every run prints a summary whose counts must
balance. The deliberately corrupted file `data/access-dirty.log` runs to the end.

### Added

| File | Purpose |
|------|---------|
| `include/logflow/core/PipelineException.hpp` | Unrecoverable error: the run stops |
| `include/logflow/core/ErrorPolicy.hpp`, `src/core/ErrorPolicy.cpp` | `FAIL_FAST`, `SKIP`, `DEAD_LETTER`, `RETRY(n)` |
| `include/logflow/core/FailedRecord.hpp` | `FailedRecord(rawInput, stageName, exception, timestamp)` for the dead-letter channel |
| `include/logflow/core/RunSummary.hpp`, `src/core/RunSummary.cpp` | Counts per run and per stage, elapsed time, balance check |
| `include/logflow/stages/ValidationStage.hpp`, `src/stages/ValidationStage.cpp` | Declarative rules: required fields, numeric ranges, timestamp range, IP format |
| `configs/dirty-dead-letter.yaml`, `configs/dirty-skip.yaml`, `configs/dirty-fail-fast.yaml`, `configs/empty-input.yaml` | Chaos-test configurations |
| `data/access-dirty.log`, `data/access-empty.log` | Corrupted and empty input |
| `tests/ChaosTest.cpp`, `tests/ValidationStageTest.cpp`, `tests/ErrorPolicyTest.cpp`, `tests/RunSummaryTest.cpp` | New test suites |

### Changed

| File | Change |
|------|--------|
| `include/logflow/core/Pipeline.hpp`, `src/core/Pipeline.cpp` | Applies the error policy of each stage, dead-letter sink, `run()` returns the `RunSummary` |
| `include/logflow/core/PipelineBuilder.hpp` | `via(stage, options)`: stage name and error policy |
| `include/logflow/stages/ParserStage.hpp`, `src/stages/ParserStage.cpp` | Throws `StageException` with the reason instead of counting silently; UTF-8 check; `maxLineLength`; comment lines; signed byte counts |
| `include/logflow/config/ConfigError.hpp` | Now a `PipelineException` |
| `src/config/PipelineLoader.cpp` | `onError` per stage, `deadLetter` sink, checks that a dead-letter sink exists when needed |
| `src/config/BuiltinComponents.cpp` | `validate` stage, `maxLineLength`, dead-letter sinks `file` and `console` |
| `include/logflow/io/LineFormat.hpp`, `src/io/LineFormat.cpp` | Dead-letter line format with escaped raw bytes |
| `src/io/FileLineSource.cpp`, `include/logflow/io/FileSink.hpp` | Throw `PipelineException`; write errors (disk full) detected |
| `src/main.cpp` | Prints the run summary, also when the run stops |
| `configs/*.yaml` | Explicit `onError: SKIP` on the parse stage |
| `ARCHITECTURE.md` | Increment 5: error model, policies, dead-letter diagram, error-to-policy table |

## v4: Configuration-Driven Composition and the Stage Registry

The pipeline is now described in a YAML configuration and built at start-up from a
registry of components. A new pipeline needs a new configuration file, not a new build.

### Added

| File | Purpose |
|------|---------|
| `include/logflow/config/StageRegistry.hpp` | `StageRegistry` and `StageFactory`: type names to factories, with item types |
| `include/logflow/config/BuiltinComponents.hpp`, `src/config/BuiltinComponents.cpp` | The registration table of all built-in components |
| `include/logflow/config/PipelineLoader.hpp`, `src/config/PipelineLoader.cpp` | Reads YAML, checks the schema, resolves types, builds the pipeline |
| `include/logflow/config/Params.hpp`, `src/config/Params.cpp` | Typed, validated access to component parameters |
| `include/logflow/config/ConfigError.hpp` | Configuration-time error |
| `include/logflow/config/ItemType.hpp` | Readable names for the item types between components |
| `src/stages/Predicates.cpp` | `compare(field, op, value)`: the configurable filter condition |
| `configs/error-report.yaml`, `configs/api-traffic.yaml`, `configs/bandwidth-by-client.yaml` | Three configurations |
| `tests/PipelineLoaderTest.cpp`, `tests/StageRegistryTest.cpp`, `tests/ConfigFilesTest.cpp`, `tests/CompareTest.cpp` | 25 new tests |
| `tests/fixtures/configs/*.yaml` | Invalid configurations used for the error-message examples |

### Changed

| File | Change |
|------|--------|
| `include/logflow/app/CommandLine.hpp`, `src/app/CommandLine.cpp` | `--config FILE` option |
| `include/logflow/app/PipelineAssembly.hpp`, `src/app/PipelineAssembly.cpp` | Uses the loader for `--config` |
| `src/main.cpp` | Reports configuration errors with exit code 3 |
| `include/logflow/stages/Predicates.hpp` | Declares `compare` and `ComparisonError` |
| `include/logflow/io/FileSink.hpp` | Creates the output directory if it does not exist |
| `CMakeLists.txt` | yaml-cpp dependency, new sources and tests |
| `Makefile`, `logflow` | Build through CMake (the program now needs yaml-cpp) |
| `tests/CommandLineTest.cpp`, `tests/IntegrationTest.cpp` | Tests for `--config` |
| `.gitignore` | Ignores `out/` |
| `README.md`, `ARCHITECTURE.md` | Configuration schema, error-message examples |

### Unchanged

`Pipeline`, `PipelineBuilder`, all interfaces, `ParserStage`, `PredicateFilterStage`,
`ProjectionStage`, `FileLineSource`, `ConsoleSink`, `LogRecord`.

## v3: Composition: Filtering, Projection and the Pipeline Builder

Three new stages, a fluent `PipelineBuilder`, and command-line options that decide which
stages are assembled. LogFlow is now a configurable command-line log filter.

### Added

| File | Purpose |
|------|---------|
| `include/logflow/stages/PredicateFilterStage.hpp` | Passes an item only if a predicate accepts it (21 lines of code) |
| `include/logflow/stages/Predicates.hpp` | `minStatus`, `pathPrefix`, `timeBetween` |
| `include/logflow/stages/ProjectionStage.hpp`, `src/stages/ProjectionStage.cpp` | Reduces a record to selected fields (36 lines of code) |
| `include/logflow/io/FileSink.hpp` | Writes to a file; opens in `open()`, flushes and closes in `close()` (35 lines of code) |
| `include/logflow/core/PipelineBuilder.hpp` | Fluent, compile-time type-checked pipeline assembly |
| `include/logflow/model/ProjectedRecord.hpp` | Output type of the projection |
| `include/logflow/model/LogRecordFields.hpp`, `src/model/LogRecordFields.cpp` | Field access by name |
| `include/logflow/io/LineFormat.hpp`, `src/io/LineFormat.cpp` | One-line text formats shared by both sinks (replaces `src/io/ConsoleSink.cpp`) |
| `include/logflow/app/CommandLine.hpp`, `src/app/CommandLine.cpp` | Command-line options |
| `include/logflow/app/PipelineAssembly.hpp`, `src/app/PipelineAssembly.cpp` | Options to pipeline, using the builder |
| `experiments/OrderingExperiment.cpp` | Stage-ordering experiment (results in `ARCHITECTURE.md`) |
| `tests/PredicateFilterStageTest.cpp`, `tests/ProjectionStageTest.cpp`, `tests/FileSinkTest.cpp`, `tests/PipelineBuilderTest.cpp`, `tests/CommandLineTest.cpp`, `tests/LineFormatTest.cpp`, `tests/IntegrationTest.cpp` | 34 new tests |
| `tests/fixtures/mini-access.log` | Integration-test fixture |

### Changed

| File | Change |
|------|--------|
| `include/logflow/core/Sink.hpp` | Sinks get `open()` / `close()` lifecycle hooks, like stages |
| `include/logflow/core/Pipeline.hpp`, `src/core/Pipeline.cpp` | Opens the sink first and closes it last, also when the run fails |
| `include/logflow/io/ConsoleSink.hpp` | Now a template, so it can print `LogRecord` or `ProjectedRecord` |
| `src/main.cpp` | Parses options, assembles, runs; prints usage on errors |
| `include/logflow/util/Timestamp.hpp`, `src/util/Timestamp.cpp` | `parseIso8601` for `--since` / `--until` |
| `tests/PipelineTest.cpp`, `tests/ConsoleSinkTest.cpp`, `tests/TimestampTest.cpp` | Tests for the changes above |
| `CMakeLists.txt`, `Makefile` | New sources, tests, fixture path, experiment target |
| `README.md`, `ARCHITECTURE.md` | Documentation for increment 3 |

### Unchanged

`ParserStage`, `FileLineSource`, `LogRecord`, `Stage`, `Source`, `Emitter`, `Record`.

### Removed

`src/io/ConsoleSink.cpp` (its formatting moved to `LineFormat`).

## v2: Typed Records and the First Real Filter

The pipeline now carries a domain model (`LogRecord`) instead of strings.
A `ParserStage` was inserted between the existing source and sink.

### Added

| File | Purpose |
|------|---------|
| `include/logflow/model/LogRecord.hpp`, `src/model/LogRecord.cpp` | Immutable domain record with a builder and an `attributes` extension map |
| `include/logflow/stages/ParserStage.hpp`, `src/stages/ParserStage.cpp` | Parses Common Log Format lines; skips and counts malformed lines |
| `include/logflow/util/Timestamp.hpp`, `src/util/Timestamp.cpp` | CLF timestamp parsing (with time zone) and ISO 8601 formatting |
| `src/io/ConsoleSink.cpp` | Record formatting moved out of the header |
| `tests/support/CollectingEmitter.hpp` | Test double that captures everything a stage emits |
| `tests/*Test.cpp` (6 files) | 35 unit tests, 15 of them for `ParserStage` |
| `scripts/coverage.sh` | Line-coverage report |
| `CHANGELOG.md` | This file |

### Changed

| File | Change |
|------|--------|
| `src/main.cpp` | Adds `ParserStage` to the pipeline (one `addStage` call) |
| `include/logflow/io/ConsoleSink.hpp` | Now a `Sink<LogRecord>` that prints one readable line per record |
| `data/access-small.log` | 6 malformed lines added to demonstrate skipping and counting |
| `CMakeLists.txt` | Library target, test target with GoogleTest, coverage option |
| `Makefile` | `test` and `coverage` targets |
| `README.md`, `ARCHITECTURE.md` | Documentation for increment 2 |

### Unchanged

`Pipeline`, `Stage`, `Source`, `Sink`, `Emitter`, `Record`, `StageException` and
`FileLineSource` did not change. The parser was added without touching the pipeline
or the source.

## v1: The Skeleton Pipeline

- Core interfaces: `Record`, `Stage`, `Emitter`, `Source`, `Sink`.
- `Pipeline` with `run()`.
- `FileLineSource` and `ConsoleSink`.
- `main` that reads the arguments, assembles the pipeline and runs it.
- Sample log `data/access-small.log`, `README.md`, `ARCHITECTURE.md`.
