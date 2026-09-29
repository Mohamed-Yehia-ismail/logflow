# LogFlow Architecture

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
