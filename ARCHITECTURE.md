# LogFlow Architecture

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
