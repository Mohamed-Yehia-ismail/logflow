# LogFlow Architecture

## Increment 4: Configuration-Driven Composition and the Stage Registry

### The goal

Until Increment 3, the shape of the pipeline was decided in C++ code
(`PipelineAssembly.cpp`). Every new kind of pipeline meant changing code and building
again. Now the pipeline is described in a **YAML file**, and the program builds it at
start-up:

```
logflow --config configs/error-report.yaml
```

**Milestone: a new pipeline does not need a new build.** Write a new YAML file, run the
same program.

This separates the **architecture description** (which components, in which order, with
which parameters: the YAML file) from the **architecture implementation** (the C++
components). The same separation as between a building plan and the bricks.

### Diagram

```
 configs/*.yaml
       |
       v
+----------------+   1. read      +---------------------------------------------+
| PipelineLoader | -------------> | YAML schema check: keys, shapes, lists      |
|                |   2. resolve   +---------------------------------------------+
|                | -------------> | StageRegistry                               |
|                |                |   sources: file                             |
|                |                |   stages:  parse, filter, project           |
|                |                |   sinks:   file, console                    |
|                |   3. build     +---------------------------------------------+
|                | -------------> | factories validate params, create components|
+----------------+                +---------------------------------------------+
       |
       v
    Pipeline  --run()-->  FileLineSource -> ParserStage -> PredicateFilterStage -> ProjectionStage -> FileSink
```

### Configuration schema

```yaml
pipeline:
  name: error-report                 # optional, used in messages

  source:                            # required: exactly one source
    type: file
    params: { path: data/access-small.log }

  stages:                            # required: a non-empty list, in order
    - type: parse
      params: { format: clf }

    - type: filter
      params: { field: status, op: ">=", value: 400 }

    - type: project
      params: { fields: [timestamp, clientIp, status, path] }

  sink:                              # required: exactly one sink
    type: file
    params: { path: out/errors.txt }
```

Every component has a `type` (required) and `params` (optional). Unknown keys are
errors, so a typo like `stage:` instead of `stages:` is reported instead of ignored.

| Kind   | `type`    | Parameters | Consumes | Produces |
|--------|-----------|------------|----------|----------|
| source | `file`    | `path` (text, required, the file must exist) | | text lines |
| stage  | `parse`   | `format` (optional: `clf` or `combined`, default `clf`) | text lines | parsed records |
| stage  | `filter`  | `field`, `op`, `value` (all required, see below) | parsed records | parsed records |
| stage  | `project` | `fields` (required, a non-empty list of field names) | parsed records | projected records |
| sink   | `file`    | `path` (text, required; the directory is created) | parsed or projected records | |
| sink   | `console` | none | parsed or projected records | |

**Filter.** `field` is one of `timestamp`, `clientIp`, `method`, `path`, `status`, `bytes`,
`userAgent`. The allowed operators and the type of `value` depend on the field:

| Field type | Fields | Operators | `value` |
|------------|--------|-----------|---------|
| number | `status`, `bytes` | `==` `!=` `<` `<=` `>` `>=` | a whole number |
| time   | `timestamp` | `==` `!=` `<` `<=` `>` `>=` | UTC time, e.g. `2026-09-01T09:00:00Z` |
| text   | `clientIp`, `method`, `path`, `userAgent` | `==` `!=` `startsWith` `endsWith` `contains` | any text |

Operators that start with `<`, `>` or `!` must be quoted in YAML (`op: ">="`), because
these characters have a special meaning there.

### StageRegistry and StageFactory

`StageRegistry` maps the type names written in the YAML file to **factories**. A
`StageFactory<S>` is a function that receives the configured parameters, validates them,
and creates a component of type `S`.

```cpp
void registerBuiltinComponents(StageRegistry& registry) {
    registry.addSource<FileLineSource>("file", fileSource);

    registry.addStage<ParserStage>("parse", parseStage);
    registry.addStage<PredicateFilterStage<>>("filter", filterStage);
    registry.addStage<ProjectionStage>("project", projectStage);

    registry.addSink<FileSink<LogRecord>>("file", fileSink<LogRecord>);
    ...
}
```

**Registration is data, not a switch.** The loader never names a concrete component; it
only looks up names in the registry. To add a new stage type, add **one entry** to this
table (in `src/config/BuiltinComponents.cpp`). The loader, the pipeline and the other
stages do not change. `StageRegistryTest.NewStageTypeIsUsableFromConfigurationWithoutChangingTheLoader`
proves this: it registers a new stage in a test and uses it from YAML immediately.

C++ has no reflection or annotations like Java's `ServiceLoader` or C#'s attributes, so
LogFlow uses the alternative the assignment allows: a single registration table. (Self-
registering static objects were not used: in a static library the linker may drop them,
and their start-up order is not defined.)

Each entry also records the **item types** its component consumes and produces. This lets
the loader check that the configured pipeline fits together *before* building anything.

### PipelineLoader: three steps, all before any data is read

1. **Read.** The YAML is parsed and checked against the schema: required keys, no unknown
   keys, `stages` is a non-empty list, parameters are single values or lists.
2. **Resolve.** Every `type` is looked up in the registry, and the item types are checked
   from source to sink (a `filter` needs parsed records, so it cannot come before `parse`).
3. **Build.** Every factory validates its parameters (required, allowed, correct type) and
   creates its component. The components are added to a `Pipeline`.

Only then does `main` call `run()`.

### Configuration-time errors vs. run-time data errors

| | Configuration-time error | Run-time data error |
|---|---|---|
| Example | unknown stage type, missing parameter, `value: abc` for `status` | a malformed line in the log file |
| Found | while loading, before any data is read | while the pipeline runs |
| Cause | the person who wrote the configuration | the data |
| Handling | **fail fast, fail loudly**: stop at once, exit code 3, message with file, line, problem and fix | skip and count, keep processing the rest (proper handling: Increment 5) |

Why fail fast? A configuration error means the pipeline is not what the user intended. Running
it anyway would produce wrong output that looks right. Stopping before reading any data also
means no half-written output files (`PipelineLoaderTest.ErrorsAreFoundBeforeAnyDataIsWritten`).

Every message has the same structure: **where** (file and line), **which component**
(`stage 2 (filter)`), **what is wrong**, and **how to fix it** (the expected parameters, the
known types, `Did you mean 'filter'?`, `Add a 'parse' stage before it.`). Examples are in the
README.

### Three configurations, one program

| Configuration | Stages | Output |
|---|---|---|
| `error-report.yaml` | parse, filter `status >= 400`, project 4 fields | `out/errors.txt` (74 lines) |
| `api-traffic.yaml` | parse, filter `path startsWith /api`, project 5 fields | `out/api-traffic.txt` (21 lines) |
| `bandwidth-by-client.yaml` | parse, filter `bytes > 0`, project client, bytes, time | `out/bandwidth-by-client.txt` (177 lines) |

`ConfigFilesTest` runs all three and checks their outputs.

### What changed

The command-line options of Increment 3 still work: `assemblePipeline` uses the loader for
`--config` and the builder otherwise, and `main` is unchanged apart from reporting
configuration errors. `Pipeline`, the interfaces and the stages did not change. The only
change to an existing component: `FileSink` now creates the output directory (`out/`) if it
does not exist. See [CHANGELOG.md](CHANGELOG.md).

---

## Increment 3: Composition: Filtering, Projection and the Pipeline Builder

### Diagram

```
                                    one per option, any number, any order
                                  +--------------------------------------+
+----------------+   +--------+   |  +----------------------+            |   +-----------------+   +-----------------+
| FileLineSource |-->| Parser |-->|  | PredicateFilterStage |--> ...    |-->| ProjectionStage |-->| FileSink        |
| string         |   | Stage  |   |  | LogRecord->LogRecord |            |   | LogRecord ->    |   | or ConsoleSink  |
+----------------+   +--------+   |  +----------------------+            |   | ProjectedRecord |   +-----------------+
                                  +--------------------------------------+   +-----------------+
                                    --min-status --path-prefix                  --fields            --out
                                    --since --until
```

### A filter and a transformer are the same architectural element

`PredicateFilterStage` removes records. `ProjectionStage` reshapes them. `ParserStage`
changes their type. All three are just a `Stage<I, O>`:

| Stage                  | Input       | Output            | Outputs per input |
|------------------------|-------------|-------------------|-------------------|
| `ParserStage`          | `string`    | `LogRecord`       | 0 or 1            |
| `PredicateFilterStage` | `LogRecord` | `LogRecord`       | 0 or 1            |
| `ProjectionStage`      | `LogRecord` | `ProjectedRecord` | exactly 1         |

This is why Increment 1 made `process` emit through an `Emitter` instead of returning a
value: dropping, transforming and type-changing stages all fit the same interface, so the
pipeline does not need to know which kind it is running. Stages can be combined in any
number and in any order, as long as each one accepts what the previous one produces.

Each stage has exactly one responsibility and stays small (lines of code, without
comments and blank lines):

| Stage                  | Lines of code |
|------------------------|--------------:|
| `PredicateFilterStage` | 21 |
| `ProjectionStage`      | 36 (header + source) |
| `FileSink`             | 35 |

`PredicateFilterStage` does not know any filtering rule: the rule is a predicate passed
in from outside (`predicates::minStatus(400)`, `pathPrefix("/api")`,
`timeBetween(from, to)`). New rules need no new stage.

### Projection and data minimization

`ProjectionStage` reduces each record to the requested fields and emits a
`ProjectedRecord`. Two reasons:

- **Output size.** Four short fields instead of a full log line.
- **Privacy.** Access logs contain personal data (IP addresses, user agents). Data
  minimization (for example under the GDPR) means passing on only what the reader
  actually needs. The raw line is deliberately not selectable, because it would bring
  back every field. Everything after the projection never sees the dropped fields.

### Lifecycle hooks are no longer decoration

Until now, `open()` and `close()` existed but did nothing. `FileSink` is the first
component that holds a resource:

- `open()` creates (or truncates) the output file.
- `consume()` writes into a 64 KB buffer in memory, not directly to disk.
- `close()` flushes the buffer to disk and closes the file.

**If `close()` is never called, the buffered output is silently lost.** The test
`FileSinkTest.OutputIsOnlyInTheFileAfterClose` makes this visible: after `consume()` the
file on disk is still empty; only `close()` writes the line.

To support this, `Sink` received the same `open()` / `close()` hooks as `Stage`, and
`Pipeline::run` now:

1. opens the sink **first**, then the stages in order,
2. runs the source,
3. closes the stages in reverse order, then the sink **last** (a stage may still emit
   while it is being closed, so the sink must still be open),
4. on failure, still closes everything that was opened, then rethrows the original error.

A note on C++: `std::ofstream` also flushes in its destructor (RAII), which hides this
bug in simple programs. Relying on it is still wrong. The destructor only runs when the
pipeline object is destroyed, which can be much later, and never when the program ends
with `std::exit` or crashes. Errors during that implicit flush are silently ignored,
while `close()` reports them as an exception.

### PipelineBuilder

```cpp
Pipeline p = PipelineBuilder::from(std::make_unique<FileLineSource>(in))
                 .via(std::make_unique<ParserStage>())
                 .via(std::make_unique<PredicateFilterStage<>>(predicates::minStatus(400)))
                 .via(std::make_unique<ProjectionStage>(fields))
                 .to(std::make_unique<FileSink<ProjectedRecord>>(out));
```

- **Fluent.** The code reads in the same order as the data flows.
- **Type-checked at compile time.** Every builder step knows the item type at that point.
  Connecting a stage or sink that expects another type is a compile error
  (`static_assert`), not a run-time error.
- **Built on `Pipeline`.** The builder only adds the fluent interface and the compile-time
  check. `Pipeline` keeps its run-time check for code that assembles it directly.

The builder is the composition mechanism for this increment. In Increment 4 the
hand-written assembly (`src/app/PipelineAssembly.cpp`) will be replaced by configuration.

### Command-line options decide the composition

`CommandLine` turns the arguments into an `Options` value. `assemblePipeline(options)`
adds one `PredicateFilterStage` per filter option, a `ProjectionStage` only when
`--fields` is given, and a `FileSink` or `ConsoleSink` depending on `--out`. `main` still
does only three things: read the arguments, assemble the pipeline, run it.

### Ordering experiment

**Question.** Does the order of stages change the result? Does it change the performance?

**Setup.** `experiments/OrderingExperiment.cpp`, run with `make experiment`.

- Data: `data/access-small.log` repeated 5000 times, giving 1,030,000 lines and
  1,000,000 valid records. Experiment 3 uses the first 103,000 lines.
- Release build (`-O3`), Apple M2, median time of 5 runs.
- In experiments 1 and 2 the records are parsed once in advance, so only the filter and
  projection work is measured. Experiment 3 includes parsing.
- Selectivity in this data: `status >= 400` keeps 37% of records, `path starts with /api`
  keeps 10.5%, both together keep 4%.
- The output of both orders is compared line by line.

#### Experiment 1: the order of two filters

| Order                          | Output records | Predicate calls | Median time |
|--------------------------------|---------------:|----------------:|------------:|
| A: status filter -> path filter | 40,000 | 1,370,000 | 205.4 ms |
| B: path filter -> status filter | 40,000 | 1,105,000 | 170.3 ms |

Identical output. B does 19% fewer predicate calls and is 17% faster. Filtering keeps a
record only if every predicate accepts it, and "and" is commutative, so the result cannot
depend on the order. The cost does: every record pays for the first filter, but only the
survivors pay for the second. **Put the most selective (and cheapest) filter first.**

#### Experiment 2: filter before or after projection

| Order                                    | Output records | Projections | Median time |
|------------------------------------------|---------------:|------------:|------------:|
| A: filter -> project                     | 370,000 | 370,000   | 384.6 ms |
| B: project -> filter (on projected text) | 370,000 | 1,000,000 | 565.5 ms |

Identical output, but B is 1.47 times slower: it projects every record, including the 63%
that are then thrown away. This is a real trade-off, not just a speed question:

- **Filter first (A)** is faster, and the filter works on typed fields (`int status`).
- **Project first (B)** only works if the projection **keeps every field the filter
  needs**. Here `status` had to stay in the output. If the user asks for
  `--fields timestamp,path`, filtering on status after projection is impossible. The filter
  also has to work on text (`std::stoi` on the projected value), which is weaker.
- **But projecting first minimizes data earlier.** If the filter were a remote service or
  a third-party component, projecting first would make sure it never sees the IP address
  or the user agent. Privacy can be worth the extra cost.

LogFlow assembles filters before the projection (A), because the filters are our own code
and need typed fields.

#### Experiment 3: experiment 1 including parsing

| Order                       | Output records | Predicate calls | Median time |
|-----------------------------|---------------:|----------------:|------------:|
| A: parse -> status -> path | 4,000 | 137,000 | 2393.5 ms |
| B: parse -> path -> status | 4,000 | 110,500 | 2381.1 ms |

Identical output, and almost the same time (B/A = 0.99). Parsing takes about 23 µs per
line, which is more than 99% of the run time, so the filter order no longer matters. The
lesson: optimize the bottleneck. In LogFlow that is the regular expression in
`ParserStage`, not the filter order.

#### Experiment 4: filtering before parsing

```
Pipeline: stage input type does not match the previous output type
```

A filter on `LogRecord` cannot run before the parser, because before parsing there is
only text and no `status` or `path` field. `Pipeline` rejects this while the pipeline is
being assembled, and with `PipelineBuilder` it does not even compile. Composition is free
only within the rules of the types.

#### Conclusions

1. Composable stages give the **same result in any valid order**, as the experiment
   confirmed for every pair.
2. **Performance does depend on the order:** a selective filter first saves work for
   everything after it.
3. **Filter before or after projection is a design trade-off** between speed and typed
   filtering (filter first) and early data minimization (project first).
4. **Parse must come before filter**, which the type system enforces.

---

## Increment 2: Typed Records and the First Real Filter

### Diagram

```
+----------------+          +----------------+             +-----------------+
| FileLineSource | -------> |  ParserStage   | ----------> |   ConsoleSink   |
| Source<string> |  string  | Stage<string,  |  LogRecord  | Sink<LogRecord> |
| reads the file |          |    LogRecord>  |             | prints records  |
+----------------+          +----------------+             +-----------------+
                             malformed lines:
                             skipped + counted,
                             total printed on close()
```

### From strings to a domain model

In Increment 1 the pipeline carried raw `std::string` lines. Every later stage would
have had to re-parse the text to find a status code or a path. Now the pipeline carries
a **domain model**, `LogRecord`, and parsing happens exactly once, in `ParserStage`.

Each component has one concern:

| Component        | Knows about                         | Does not know about        |
|------------------|-------------------------------------|----------------------------|
| `FileLineSource` | files and lines                     | log formats, records       |
| `ParserStage`    | the Common Log Format               | files, consoles            |
| `ConsoleSink`    | how to display a `LogRecord`        | where records come from    |
| `Pipeline`       | the order of components             | any concrete component     |

### LogRecord

```cpp
class LogRecord : public Record {
    Timestamp timestamp; std::string clientIp, method, path;
    int status; long long bytes; std::string userAgent;
    std::map<std::string, std::string> attributes;
    std::string raw;
};
```

- **Immutable.** Fields are private with const accessors only. A record is created in
  one step with `LogRecord::Builder`. Stages can never modify a record that another
  stage is still holding; to "change" a record a stage creates a new one with
  `withAttribute`.
- **`raw`** keeps the original line, so nothing is lost by parsing.
- **`attributes` is the extension point.** Later stages (weeks 5 to 7) will attach data
  that today's parser knows nothing about, such as an enrichment or a classification.
  An open key/value map means those stages can enrich records **without changing
  `LogRecord`, `ParserStage` or any other stage**: the record format is open for
  extension and closed for modification. The parser already uses it for the optional
  `referer` field.
- **Timestamps** are converted to UTC when parsed, so records from servers in different
  time zones compare correctly.

### ParserStage

Accepts the Common Log Format, and the Combined Log Format extension
(`"referer" "user-agent"`):

```
host ident authuser [dd/Mon/yyyy:HH:MM:SS +zzzz] "METHOD path PROTOCOL" status bytes ["referer" "user-agent"]
```

A line is **malformed** if it does not match this shape, the date does not exist, the
request line does not have exactly three parts, the status code is not a number from
100 to 599, or the byte count is not a number or `-`.

**Malformed lines are only skipped and counted in this increment.** The stage uses the
`close()` lifecycle hook (unused since Increment 1) to print the total at the end of the
run. Proper handling of malformed lines (reporting why a line is bad, sending it to a
separate output) is planned for **week 5**. The architecture is built in layers: this
increment establishes where that decision lives (inside the parser stage) without
building the full mechanism yet.

### What changed to add the parser

Only the code that *assembles* the pipeline and the sink that *displays* records:

- `src/main.cpp`: one `addStage(...)` call.
- `ConsoleSink`: now prints a `LogRecord` instead of a string (a task requirement).

`Pipeline`, all five interfaces and `FileLineSource` were **not changed**. A new stage was
inserted into the middle of an existing pipeline and its neighbours did not notice.
See [CHANGELOG.md](CHANGELOG.md) for the full list.

### Testing without files

Stage tests do not read any file. A stage depends only on the `Emitter` interface, not on
the component that comes after it. So a test can:

1. create the stage,
2. call `process` with a string,
3. pass a `CollectingEmitter` (a test double that stores what it receives),
4. check the stored records.

This matters because the tests are fast, have no setup or clean-up, cannot fail because
of the environment, and test the parser in isolation. It is a direct result of the
architecture: components talk through small interfaces, so any neighbour can be
replaced by a fake. `CollectingEmitter` will be reused for every stage in later weeks.

---

## Increment 1: The Skeleton Pipeline

### Diagram

```
+------------------+ Emitter<std::string> +-----------------+
|  FileLineSource  | -------------------> |   ConsoleSink   |
|  Source<string>  |    one line at a     |  Sink<string>   |
|  reads the file  |    time              |  prints lines   |
+------------------+                      +-----------------+
          \________________ Pipeline::run() ________________/
```

In general, a pipeline has zero or more stages between the source and the sink:

```
Source --emit--> Stage 1 --emit--> Stage 2 --emit--> ... --emit--> Sink
```

In Increment 1 the list of stages is empty, so the source feeds the sink directly.

### Vocabulary

| Term       | Meaning                                                            |
|------------|--------------------------------------------------------------------|
| `Record`   | A unit of data moving through the pipeline. In Increment 1 a record is a plain `std::string` line. The `Record` interface is defined now so the name is fixed for later increments, which will parse lines into structured records. |
| `Source`   | Component at the start. It produces items and emits them downstream. |
| `Stage`    | Component in the middle. It processes one input and emits zero or more outputs. |
| `Sink`     | Component at the end. It consumes every item that reaches it.     |
| `Emitter`  | The connector. A component hands an item to the next one by calling `emit`. |
| `Pipeline` | Assembles one source, an ordered list of stages and one sink, and runs them. |

### Components and connectors

- **Components** do the work: `FileLineSource`, `ConsoleSink`, and later every `Stage`.
- **Connectors** move data between components: the `Emitter`. Components never call
  each other directly. They only know the `Emitter` they were given.
- **`Pipeline`** is the only class that knows the order of the components. It builds the
  chain of emitters from the sink backwards and then asks the source to produce.

### Interface vs. implementation

The `core` directory contains only interfaces, written as abstract class templates
with pure virtual methods (plus the `Pipeline` that wires them). It does not depend
on files, consoles, or log formats. The `io` directory contains implementations of
those interfaces.

| Interface (what)      | Implementation (how)                              |
|-----------------------|---------------------------------------------------|
| `Source<std::string>` | `FileLineSource`: read a file with `std::ifstream` and `std::getline` |
| `Sink<std::string>`   | `ConsoleSink`: write each line to a `std::ostream` |

Because `Pipeline` depends only on the interfaces, a new source (for example, reading
from standard input or a network socket) or a new sink (for example, writing to a file)
can be added without changing `Pipeline` or any existing component.

### Design decisions

1. **`Stage::process` emits instead of returning a value.**
   A stage may produce zero, one or many outputs for each input. A filter drops lines,
   and a splitter turns one line into several. A return value would force a strict 1:1
   mapping.

2. **Push model.** The source pushes each item through the whole chain before it reads
   the next one. The file is never loaded into memory at once, so large logs are fine.

3. **Small interfaces.** Each interface has a single method (plus optional lifecycle
   hooks on `Stage`). Small interfaces are easy to implement, easy to test, and easy
   to replace.

4. **Lifecycle hooks.** `Stage::open()` and `Stage::close()` are virtual with empty
   default implementations. `Pipeline` already calls them (opens in order, closes in reverse)
   so that later stages which hold resources work without changing the pipeline.

5. **`main` has no business logic.** It does exactly three things: read the
   command-line arguments, assemble the pipeline, and call `run()`. It only adds
   a `try`/`catch` that turns an error into a message and a non-zero exit code.

6. **Errors.** A stage signals failure by throwing `StageException`. The exception
   travels back through the chain and stops the run. The pipeline still closes every
   stage it opened before passing the exception on.

7. **Type-safe assembly with type erasure.** Stages may change the item type (for
   example `std::string` to a parsed record in a later increment), so a single
   `std::vector` of stages cannot hold one fixed type. `Pipeline` stores each
   component behind a `std::function` and passes items between them as `std::any`.
   To keep this safe, every interface exposes its item types (`InputType`,
   `OutputType`), and `Pipeline` checks while it is being assembled that each stage
   accepts what the previous component produces. A mismatch throws
   `std::logic_error` before any data flows.

8. **Ownership.** Components are handed to the pipeline as `std::unique_ptr`, so the
   pipeline owns them and frees them automatically.

### Why the pipeline looks like overkill

For Increment 1 alone, a three-line `std::getline` loop in `main` would do the same job. The value of
the pipeline shows up in later increments, when stages (parsing, filtering,
aggregation) are added by composing new components instead of rewriting existing code.
This is normal for an architecture in its first week.
