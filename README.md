# LogFlow

LogFlow is a small log-processing system built as a **pipeline** of components:
a *source* produces records, *stages* transform or filter them, and a *sink* consumes them.

Current version: **Increment 5: Error Handling, Validation and the Dead-Letter Channel**
(tag `v5`). Bad records no longer disappear silently: each stage has an error policy
(`FAIL_FAST`, `SKIP`, `DEAD_LETTER`, `RETRY(n)`), failed records can go to a separate
dead-letter sink, a validation stage checks records against rules from the
configuration, and every run ends with a summary whose counts must add up.

```
source ──> parse ──> validate ──> ... ──> sink
             │          │
             └──────────┴──> dead-letter sink
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

### Corrupted input (Increment 5)

`data/access-dirty.log` is a deliberately corrupted log: truncated lines, a negative byte
count, timestamps from 1970 and 2099, bytes that are not UTF-8 and a 100 KiB line.
`data/access-empty.log` is empty. The same file with three error policies:

```bash
./logflow --config configs/dirty-dead-letter.yaml   # valid -> out/dirty-valid.txt, bad -> out/dead-letter.txt
./logflow --config configs/dirty-skip.yaml          # bad records dropped and counted
./logflow --config configs/dirty-fail-fast.yaml     # stops at the first bad record
./logflow --config configs/empty-input.yaml         # empty file: 0 records, no crash
```

Every run ends with a summary (on stderr):

```
Run summary: dirty-dead-letter
  records read           11
  stage       policy            in    out  filtered  skipped  dead-letter
  1. parse    DEAD_LETTER      11      6         1        0            4
  2. validate DEAD_LETTER       6      3         0        0            3
  records written         3
  dead-lettered           7
  elapsed           4.335 ms
  check: 11 read = 3 written + 1 dropped + 7 dead-lettered  OK
```

Error policies and validation rules are configured per stage:

```yaml
stages:
  - type: parse
    onError: DEAD_LETTER           # FAIL_FAST, SKIP (default), DEAD_LETTER or RETRY(n)
  - type: validate
    params:
      required: [clientIp, method, path]
      status: 100..599
      bytes: 0..
      timestamp: 2000-01-01T00:00:00Z..2030-12-31T23:59:59Z
      clientIp: ip                 # ipv4, ipv6 or ip
    onError: DEAD_LETTER
deadLetter:                        # next to 'sink'
  type: file
  params: { path: out/dead-letter.txt }
```

Which policy handles which error in the corrupted file is listed in
[ARCHITECTURE.md](ARCHITECTURE.md#chaos-test-every-error-in-the-corrupted-file-and-the-policy-that-handles-it).

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
| 1 | the run stopped (`PipelineException`: unreadable input, output cannot be written, or a stage failed under `FAIL_FAST`) |
| 2 | invalid command line |
| 3 | invalid configuration |

## Error messages

Configuration errors are found **before any data is read** and stop the program with a
message that says where the problem is (file and line) and how to fix it. The example
configurations below are in `tests/fixtures/configs/`.

**Unknown stage type**

```
$ ./logflow --config tests/fixtures/configs/unknown-stage.yaml
logflow: configuration error: tests/fixtures/configs/unknown-stage.yaml:8: stage 2 (filtr): unknown stage type 'filtr'. Known stage types: filter, parse, project, validate. Did you mean 'filter'?
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
unknown key 'stage' in 'pipeline' (allowed: name, source, stages, sink, deadLetter).
'pipeline' is missing the required key 'source'.
unknown parameter 'strict'. Expected parameters: format, maxLineLength.
unknown error policy 'IGNORE' (use FAIL_FAST, SKIP, DEAD_LETTER or RETRY(n) with n from 1 to 10).
onError DEAD_LETTER needs a dead-letter sink, but the pipeline has none. Add for example ...
'100' is not a range. Write min..max, for example 100..599.
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

144 tests in `tests/`, written with GoogleTest:

| Suite                      | Tests | What it covers |
|----------------------------|------:|----------------|
| `ChaosTest`                |  4 | **the corrupted and the empty file** with `DEAD_LETTER`, `SKIP` and `FAIL_FAST`: valid records reach the main sink, each bad record is accounted for, counts balance |
| `PipelineTest`             | 21 | stage order, lifecycle, **every error policy**, dead-letter routing, retries, summary counts |
| `ValidationStageTest`      |  8 | required fields, numeric ranges, timestamp range, IP format, all broken rules reported |
| `ErrorPolicyTest`          |  4 | policy names, `RETRY(n)` |
| `RunSummaryTest`           |  5 | counts, balance check, printed table |
| `PipelineLoaderTest`       | 21 | valid configuration, unknown stage type, missing parameter, wrong parameter type, empty pipeline, impossible order, `onError`, `deadLetter`, `validate` parameters |
| `StageRegistryTest`        |  5 | built-in entries, item types, duplicates, a new stage type usable without loader changes |
| `ConfigFilesTest`          |  3 | the three report configurations in `configs/` and their outputs |
| `CompareTest`              |  4 | `field op value` filters on numbers, times and text |
| `ParserStageTest`          | 20 | CLF parsing, the reason for each rejected line, UTF-8, line-length limit, comments |
| `PredicateFilterStageTest` |  7 | filtering |
| `ProjectionStageTest`      |  5 | projection |
| `FileSinkTest`             |  6 | file output and `close()` |
| `PipelineBuilderTest`      |  2 | fluent assembly |
| `CommandLineTest`          |  5 | options, `--config` |
| `IntegrationTest`          |  4 | fixture through full pipelines, from options and from YAML |
| `TimestampTest`            |  8 | time parsing |
| `LogRecordTest`            |  4 | the record |
| `ConsoleSinkTest`          |  3 | console output |
| `LineFormatTest`           |  3 | output format, dead-letter lines |
| `FileLineSourceTest`       |  2 | file input |

## Coverage

Measured with `make coverage` (clang source-based coverage, `llvm-cov`) over the library
sources in `include/` and `src/`:

| Metric            | v2     | v3     | v4     | v5 |
|-------------------|--------|--------|--------|----|
| **Line coverage** | 99.66% | 99.81% | 99.89% | **99.87%** (1487 of 1489 lines) |
| Function coverage | 98.46% | 100.00% | 100.00% | 100.00% (232 of 232 functions) |

## Project layout

```
logflow/
├── logflow                         launcher script
├── Makefile  CMakeLists.txt  CHANGELOG.md  README.md  ARCHITECTURE.md
├── configs/                        report configurations, dirty-*.yaml, empty-input.yaml
├── data/                           access-small.log, access-dirty.log, access-empty.log
├── include/logflow/
│   ├── core/                       interfaces, Pipeline, PipelineBuilder, errors, RunSummary
│   ├── config/                     StageRegistry, PipelineLoader, Params, BuiltinComponents
│   ├── model/                      LogRecord, ProjectedRecord
│   ├── stages/                     ParserStage, ValidationStage, PredicateFilterStage, Predicates, ProjectionStage
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
| `v5` | 5: Error Handling, Validation and the Dead-Letter Channel |
