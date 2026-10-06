# LogFlow

LogFlow is a small log-processing system built as a **pipeline** of components:
a *source* produces records, *stages* transform or filter them, and a *sink* consumes them.

Current version: **Increment 2: Typed Records and the First Real Filter** (tag `v2`).
The pipeline reads an access log, parses every line into a typed `LogRecord`, and prints
one readable line per record. Malformed lines are skipped, counted, and the total is
printed at the end.

```
FileLineSource ──string──▶ ParserStage ──LogRecord──▶ ConsoleSink
```

## Requirements

- A C++17 compiler (clang++ or g++)
- `make` to build and run the program
- CMake 3.16+ and GoogleTest to run the tests (GoogleTest is downloaded automatically
  by CMake if it is not installed)
- clang with `llvm-profdata` and `llvm-cov` for the coverage report

## Build and run

```bash
./logflow data/access-small.log
```

Example output:

```
2026-09-01T08:01:22Z  192.0.2.38  GET /contact  200  18224 B  "Mozilla/5.0 (Macintosh; ...)"
2026-09-01T08:01:36Z  203.0.113.54  GET /robots.txt  404  35941 B  "Mozilla/5.0 (Windows NT 10.0; ...)"
...
Malformed lines skipped: 6
```

The sample file `data/access-small.log` has 206 lines: 200 valid entries and
6 deliberately malformed ones (empty line, missing field, impossible date, bad status
code, free text, unterminated request).

Other commands:

```bash
make            # builds build/logflow
make run        # builds and runs on data/access-small.log
make test       # builds and runs the unit tests (CMake + GoogleTest)
make coverage   # runs the tests with coverage and prints the line-coverage report
make clean      # removes build/
```

With CMake only:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake
./build/cmake/logflow data/access-small.log
```

## Tests

35 unit tests in `tests/`, written with GoogleTest:

| Suite                | Tests | What it covers |
|----------------------|------:|----------------|
| `ParserStageTest`    | 15 | valid line, missing field, malformed timestamp, malformed status, empty line, extra whitespace, quoted user agent with spaces, query string, plus time zones, `-` bytes, escaping, counting and the close report |
| `PipelineTest`       |  6 | stage order, type changes, lifecycle hooks, type checking, failures |
| `TimestampTest`      |  6 | CLF timestamp parsing, offsets, leap years, invalid input |
| `LogRecordTest`      |  4 | builder, immutability, attributes |
| `ConsoleSinkTest`    |  2 | output format |
| `FileLineSourceTest` |  2 | reading lines, missing file |

Stage tests never touch the file system. They create the stage, give it a string, and
capture what it emits with the `CollectingEmitter` test double
(`tests/support/CollectingEmitter.hpp`).

## Coverage

Measured with `make coverage` (clang source-based coverage, `llvm-cov`), over the
library sources in `include/` and `src/` (test code and GoogleTest excluded,
`src/main.cpp` is not linked into the tests):

| Metric         | Result |
|----------------|--------|
| **Line coverage** | **99.66%** (293 of 294 lines) |
| Function coverage | 98.46% (64 of 65 functions) |

The one uncovered line is the empty default `Stage::close()` hook.

## Project layout

```
logflow/
├── logflow                         launcher script
├── Makefile
├── CMakeLists.txt
├── CHANGELOG.md
├── data/access-small.log           sample access log
├── include/logflow/
│   ├── core/                       architecture: interfaces and the Pipeline
│   │   ├── Record.hpp  Stage.hpp  Emitter.hpp  Source.hpp  Sink.hpp
│   │   ├── StageException.hpp
│   │   └── Pipeline.hpp
│   ├── model/LogRecord.hpp         the domain record
│   ├── stages/ParserStage.hpp      CLF parser
│   ├── io/                         FileLineSource.hpp  ConsoleSink.hpp
│   └── util/Timestamp.hpp          CLF timestamp parsing and ISO 8601 formatting
├── src/                            implementations (.cpp) and main.cpp
├── tests/                          unit tests and tests/support/CollectingEmitter.hpp
├── scripts/coverage.sh
├── README.md
└── ARCHITECTURE.md
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and [CHANGELOG.md](CHANGELOG.md)
for what changed in each increment.

## Versions

| Tag  | Increment                                   |
|------|---------------------------------------------|
| `v1` | 1: The Skeleton Pipeline                    |
| `v2` | 2: Typed Records and the First Real Filter  |
