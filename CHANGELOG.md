# Changelog

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
