# LogFlow

LogFlow is a small log-processing system built as a **pipeline** of components:
a *source* produces records, *stages* transform or filter them, and a *sink* consumes them.

Current version: **Increment 4: Configuration-Driven Composition and the Stage Registry**
(tag `v4`). The pipeline is now described in a YAML file. A new pipeline needs a new
configuration file, not a new build.

```
configs/error-report.yaml ──> PipelineLoader ──> StageRegistry ──> Pipeline ──> run
```

## Requirements

- A C++17 compiler (clang++ or g++)
- CMake 3.16+
- yaml-cpp and GoogleTest (both are downloaded automatically by CMake if they are not installed)
- clang with `llvm-profdata` and `llvm-cov` for the coverage report

## Usage

```
./logflow --config <pipeline.yaml>
./logflow <log-file> [options]
```

The `logflow` script builds the program when needed (with CMake) and runs it.

### With a configuration file

```bash
./logflow --config configs/error-report.yaml
cat out/errors.txt
```

Three configurations come with the project. The same, unchanged program produces three
different outputs:

| Configuration                        | What it produces                                      | Output                         | Lines |
|--------------------------------------|-------------------------------------------------------|--------------------------------|------:|
| `configs/error-report.yaml`          | failed requests (status >= 400): time, client, status, path | `out/errors.txt`         | 74 |
| `configs/api-traffic.yaml`           | calls to `/api`: time, method, path, status, bytes   | `out/api-traffic.txt`          | 21 |
| `configs/bandwidth-by-client.yaml`   | requests that sent data (bytes > 0): client, bytes, time | `out/bandwidth-by-client.txt` | 177 |

The configuration format is documented in [ARCHITECTURE.md](ARCHITECTURE.md#configuration-schema).

### With command-line options (Increment 3)

```
  --min-status N        keep records with status >= N
  --path-prefix P       keep records whose path starts with P
  --since TIME          keep records at or after TIME (e.g. 2026-09-01T09:00:00Z)
  --until TIME          keep records before TIME
  --fields a,b,c        output only these fields
  --out FILE            write to FILE instead of the console
```

`--config` cannot be combined with these options: the configuration describes the whole
pipeline.

### Exit codes

| Code | Meaning |
|-----:|---------|
| 0 | success |
| 1 | run-time error (for example, the output file cannot be written) |
| 2 | invalid command line |
| 3 | invalid configuration |

## Error messages

Configuration errors are found **before any data is read** and stop the program with a
message that says where the problem is (file and line) and how to fix it. The example
configurations below are in `tests/fixtures/configs/`.

**Unknown stage type**

```
$ ./logflow --config tests/fixtures/configs/unknown-stage.yaml
logflow: configuration error: tests/fixtures/configs/unknown-stage.yaml:8: stage 2 (filtr): unknown stage type 'filtr'. Known stage types: filter, parse, project. Did you mean 'filter'?
```

**Missing parameter**

```
$ ./logflow --config tests/fixtures/configs/missing-param.yaml
logflow: configuration error: tests/fixtures/configs/missing-param.yaml:8: stage 2 (filter): missing required parameter 'value'. Expected parameters: field, op, value.
```

**Parameter of the wrong type**

```
$ ./logflow --config tests/fixtures/configs/wrong-type.yaml
logflow: configuration error: tests/fixtures/configs/wrong-type.yaml:9: stage 2 (filter): value 'four-hundred' for field 'status' must be a whole number.
```

**Empty pipeline**

```
$ ./logflow --config tests/fixtures/configs/empty-pipeline.yaml
logflow: configuration error: tests/fixtures/configs/empty-pipeline.yaml:6: pipeline 'empty-pipeline' has no stages. Add at least a 'parse' stage.
```

**Stages in an impossible order**

```
$ ./logflow --config tests/fixtures/configs/filter-before-parse.yaml
logflow: configuration error: tests/fixtures/configs/filter-before-parse.yaml:7: stage 1 (filter): this stage needs parsed records, but the source produces text lines. Add a 'parse' stage before it.
```

Other configuration errors reported the same way:

```
unknown key 'stage' in 'pipeline' (allowed: name, source, stages, sink).
'pipeline' is missing the required key 'source'.
unknown parameter 'strict'. Expected parameters: format.
parameter 'fields' must be a list, for example fields: [a, b].
operator '>' is not supported for text field 'path' (use ==, !=, startsWith, endsWith, contains).
unknown field 'password' (use timestamp, clientIp, method, path, status, bytes or userAgent).
input file 'data/missing.log' does not exist.
invalid YAML: end of map flow not found.
the configuration is empty.
```

## Build commands

```bash
make              # builds build/cmake/logflow
make run          # runs configs/error-report.yaml and prints out/errors.txt
make test         # builds and runs all tests
make coverage     # runs the tests with coverage and prints the report
make experiment   # runs the stage-ordering experiment from Increment 3
make clean        # removes build/ and out/
```

## Tests

96 tests in `tests/`, written with GoogleTest:

| Suite                      | Tests | What it covers |
|----------------------------|------:|----------------|
| `PipelineLoaderTest`       | 13 | **valid configuration, unknown stage type, missing parameter, wrong parameter type, empty pipeline**, impossible order, unknown keys and types, invalid values, invalid YAML, nothing written on error |
| `StageRegistryTest`        |  5 | built-in entries, item types, duplicates, a new stage type usable without loader changes |
| `ConfigFilesTest`          |  3 | the three configurations in `configs/` and their outputs |
| `CompareTest`              |  4 | `field op value` filters on numbers, times and text |
| `ParserStageTest`          | 15 | CLF parsing |
| `PredicateFilterStageTest` |  7 | filtering |
| `ProjectionStageTest`      |  5 | projection |
| `FileSinkTest`             |  6 | file output and `close()` |
| `PipelineTest`             |  8 | stage order, lifecycle, failures |
| `PipelineBuilderTest`      |  2 | fluent assembly |
| `CommandLineTest`          |  5 | options, `--config` |
| `IntegrationTest`          |  4 | fixture through full pipelines, from options and from YAML |
| `TimestampTest`            |  8 | time parsing |
| `LogRecordTest`            |  4 | the record |
| `ConsoleSinkTest`          |  3 | console output |
| `LineFormatTest`           |  2 | output format |
| `FileLineSourceTest`       |  2 | file input |

## Coverage

Measured with `make coverage` (clang source-based coverage, `llvm-cov`) over the library
sources in `include/` and `src/`:

| Metric            | v2     | v3     | v4 |
|-------------------|--------|--------|----|
| **Line coverage** | 99.66% | 99.81% | **99.89%** (935 of 936 lines) |
| Function coverage | 98.46% | 100.00% | 100.00% (175 of 175 functions) |

## Project layout

```
logflow/
├── logflow                         launcher script
├── Makefile  CMakeLists.txt  CHANGELOG.md  README.md  ARCHITECTURE.md
├── configs/                        error-report.yaml, api-traffic.yaml, bandwidth-by-client.yaml
├── data/access-small.log           sample access log
├── include/logflow/
│   ├── core/                       interfaces, Pipeline, PipelineBuilder
│   ├── config/                     StageRegistry, PipelineLoader, Params, BuiltinComponents
│   ├── model/                      LogRecord, ProjectedRecord
│   ├── stages/                     ParserStage, PredicateFilterStage, Predicates, ProjectionStage
│   ├── io/                         FileLineSource, ConsoleSink, FileSink
│   ├── app/                        command line and assembly
│   └── util/                       timestamps
├── src/                            implementations and main.cpp
├── tests/                          tests, fixtures (logs and invalid configurations)
├── experiments/                    stage-ordering experiment
└── scripts/coverage.sh
```

## Versions

| Tag  | Increment |
|------|-----------|
| `v1` | 1: The Skeleton Pipeline |
| `v2` | 2: Typed Records and the First Real Filter |
| `v3` | 3: Composition: Filtering, Projection and the Pipeline Builder |
| `v4` | 4: Configuration-Driven Composition and the Stage Registry |
