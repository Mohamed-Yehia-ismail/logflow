# LogFlow

LogFlow is a small log-processing system built as a **pipeline** of components:
a *source* produces records, *stages* transform or filter them, and a *sink* consumes them.

Current version: **Increment 3: Composition: Filtering, Projection and the Pipeline Builder**
(tag `v3`). LogFlow is now a configurable command-line log filter: it reads, parses,
filters, projects and writes.

```
FileLineSource -> ParserStage -> [PredicateFilterStage ...] -> [ProjectionStage] -> FileSink | ConsoleSink
```

## Requirements

- A C++17 compiler (clang++ or g++)
- `make` to build and run the program
- CMake 3.16+ and GoogleTest to run the tests (GoogleTest is downloaded automatically
  by CMake if it is not installed)
- clang with `llvm-profdata` and `llvm-cov` for the coverage report

## Usage

```
./logflow <log-file> [options]

  --min-status N        keep records with status >= N
  --path-prefix P       keep records whose path starts with P
  --since TIME          keep records at or after TIME (e.g. 2026-09-01T09:00:00Z)
  --until TIME          keep records before TIME
  --fields a,b,c        output only these fields: timestamp, clientIp, method,
                        path, status, bytes, userAgent
  --out FILE            write to FILE instead of the console
  --help                show this help
```

Examples:

```bash
# Every record, printed in full
./logflow data/access-small.log

# Failed API calls, reduced to four fields
./logflow data/access-small.log --min-status 400 --path-prefix /api --fields timestamp,clientIp,status,path

# Server errors in one hour, written to a file
./logflow data/access-small.log --min-status 500 --since 2026-09-01T09:00:00Z --until 2026-09-01T10:00:00Z --out errors.txt
```

Example output with `--fields`:

```
timestamp=2026-09-01T08:03:37Z clientIp=192.0.2.15 status=500 path=/api/v1/orders
timestamp=2026-09-01T08:12:11Z clientIp=192.0.2.38 status=500 path=/api/v1/orders/1001
...
Malformed lines skipped: 6
```

Projected records are written as `name=value` pairs (the logfmt style). Values that
contain spaces, quotes or `=` are quoted. Without `--fields`, the full record is printed
as in Increment 2. The malformed-line count is written to standard error at the end.

The sample file `data/access-small.log` has 206 lines: 200 valid entries and
6 deliberately malformed ones.

## Build commands

```bash
make              # builds build/logflow
make run          # builds and runs on data/access-small.log
make test         # builds and runs the unit tests (CMake + GoogleTest)
make coverage     # runs the tests with coverage and prints the line-coverage report
make experiment   # runs the stage-ordering experiment (Release build)
make clean        # removes build/
```

With CMake only:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake
./build/cmake/logflow data/access-small.log
```

## Tests

69 unit and integration tests in `tests/`, written with GoogleTest:

| Suite                      | Tests | What it covers |
|----------------------------|------:|----------------|
| `ParserStageTest`          | 15 | CLF parsing: valid line, missing field, bad timestamp, bad status, empty line, extra whitespace, quoted user agent, query string, and more |
| `PredicateFilterStageTest` |  7 | pass/drop, `minStatus`, `pathPrefix`, `timeBetween` boundaries, other item types |
| `ProjectionStageTest`      |  5 | field selection and order, dropped fields, unknown fields |
| `FileSinkTest`             |  6 | output lines, **output only appears after `close()`**, truncation, open errors |
| `PipelineBuilderTest`      |  2 | fluent assembly, conditional steps |
| `PipelineTest`             |  8 | stage order, type changes, lifecycle order of stages and sink, failures |
| `CommandLineTest`          |  4 | every option, any order, invalid command lines |
| `IntegrationTest`          |  3 | fixture file through parse, filter and projection into a real output file |
| `TimestampTest`            |  8 | CLF and ISO 8601 parsing, offsets, leap years |
| `LogRecordTest`            |  4 | builder, immutability, attributes |
| `ConsoleSinkTest`          |  3 | output format |
| `LineFormatTest`           |  2 | `name=value` format and quoting |
| `FileLineSourceTest`       |  2 | reading lines, missing file |

Stage tests never touch the file system: they create the stage, call `process`, and
capture the output with the `CollectingEmitter` test double
(`tests/support/CollectingEmitter.hpp`). The integration test uses the fixture
`tests/fixtures/mini-access.log` (8 lines, one malformed).

## Coverage

Measured with `make coverage` (clang source-based coverage, `llvm-cov`), over the
library sources in `include/` and `src/` (test code, experiments and GoogleTest
excluded; `src/main.cpp` is not linked into the tests):

| Metric            | v2     | v3 |
|-------------------|--------|----|
| **Line coverage** | 99.66% | **99.81%** (532 of 533 lines) |
| Function coverage | 98.46% | 100.00% (104 of 104 functions) |

The one uncovered line is the console branch of `assemblePipeline`, which writes to the
real standard output.

## Project layout

```
logflow/
├── logflow                         launcher script
├── Makefile  CMakeLists.txt  CHANGELOG.md  README.md  ARCHITECTURE.md
├── data/access-small.log           sample access log
├── include/logflow/
│   ├── core/                       interfaces, Pipeline, PipelineBuilder
│   ├── model/                      LogRecord, ProjectedRecord, LogRecordFields
│   ├── stages/                     ParserStage, PredicateFilterStage, Predicates, ProjectionStage
│   ├── io/                         FileLineSource, ConsoleSink, FileSink, LineFormat
│   ├── app/                        CommandLine, PipelineAssembly
│   └── util/Timestamp.hpp
├── src/                            implementations (.cpp) and main.cpp
├── tests/                          tests, support/CollectingEmitter.hpp, fixtures/
├── experiments/OrderingExperiment.cpp
└── scripts/coverage.sh
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and the ordering experiment, and
[CHANGELOG.md](CHANGELOG.md) for what changed in each increment.

## Versions

| Tag  | Increment                                                     |
|------|---------------------------------------------------------------|
| `v1` | 1: The Skeleton Pipeline                                      |
| `v2` | 2: Typed Records and the First Real Filter                    |
| `v3` | 3: Composition: Filtering, Projection and the Pipeline Builder |
