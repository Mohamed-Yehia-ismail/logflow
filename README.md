# LogFlow

LogFlow is a small log-processing system built as a **pipeline** of components:
a *source* produces records, *stages* transform or filter them, and a *sink* consumes them.

This is **Increment 1: The Skeleton Pipeline**. The system reads a text file and prints
every line to the console. It uses no stages yet. The point of this increment is the
architecture, not the features.

## Requirements

- A C++17 compiler (clang++ or g++)
- `make` (or CMake 3.16+)

## Build and run

```bash
./logflow data/access-small.log
```

The `logflow` script builds the program with `make` when the sources have changed,
then runs `build/logflow`.

Other ways to build:

```bash
make                 # builds build/logflow
make run             # builds and runs on data/access-small.log
make clean           # removes build/
```

With CMake:

```bash
cmake -S . -B cmake-build
cmake --build cmake-build
./cmake-build/logflow data/access-small.log
```

## Project layout

```
logflow/
├── logflow                         launcher script
├── Makefile
├── CMakeLists.txt
├── data/access-small.log           sample access log (200 lines)
├── include/logflow/
│   ├── core/                       the architecture: interfaces and the Pipeline
│   │   ├── Record.hpp
│   │   ├── Stage.hpp
│   │   ├── Emitter.hpp
│   │   ├── Source.hpp
│   │   ├── Sink.hpp
│   │   ├── StageException.hpp
│   │   └── Pipeline.hpp
│   └── io/                         concrete components
│       ├── FileLineSource.hpp
│       └── ConsoleSink.hpp
├── src/
│   ├── main.cpp                    reads args, assembles the pipeline, runs it
│   ├── core/Pipeline.cpp
│   └── io/FileLineSource.cpp
├── README.md
└── ARCHITECTURE.md
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design.

## Versions

| Tag  | Increment                     |
|------|-------------------------------|
| `v1` | 1: The Skeleton Pipeline      |
