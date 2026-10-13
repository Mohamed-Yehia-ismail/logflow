# Changelog

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
