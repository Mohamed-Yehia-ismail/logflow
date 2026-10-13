// Ordering experiment for Increment 3.
//
// Runs the same data through pipelines whose stages are composed in different
// orders, checks that the results are identical, and measures the work done and
// the time taken. Results are written up in ARCHITECTURE.md.
//
// Usage: logflow_ordering_experiment [log-file] [copies]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "logflow/core/PipelineBuilder.hpp"
#include "logflow/io/FileLineSource.hpp"
#include "logflow/io/LineFormat.hpp"
#include "logflow/stages/ParserStage.hpp"
#include "logflow/stages/Predicates.hpp"
#include "logflow/stages/ProjectionStage.hpp"

using namespace logflow;

namespace {

constexpr int kRuns = 5;

template <typename T>
class VectorSource : public Source<T> {
public:
    explicit VectorSource(const std::vector<T>& items) : items_(items) {}
    void produce(Emitter<T>& out) override {
        for (const T& item : items_) out.emit(item);
    }

private:
    const std::vector<T>& items_;
};

template <typename T>
class LineCollector : public Sink<T> {
public:
    explicit LineCollector(std::vector<std::string>& into) : into_(into) {}
    void consume(const T& item) override {
        if constexpr (std::is_same_v<T, std::string>) {
            into_.push_back(item);
        } else {
            into_.push_back(formatLine(item));
        }
    }

private:
    std::vector<std::string>& into_;
};

// A predicate that counts how often it is evaluated.
template <typename T>
std::function<bool(const T&)> counted(std::function<bool(const T&)> predicate, long& calls) {
    return [predicate = std::move(predicate), &calls](const T& item) {
        ++calls;
        return predicate(item);
    };
}

struct Result {
    std::vector<std::string> output;
    long work = 0;  // predicate evaluations or projections, depending on the experiment
    double millis = 0;
};

// Runs `variant` kRuns times; keeps the output and work of the last run, the median time.
Result measure(const std::function<Result()>& variant) {
    std::vector<double> times;
    Result result;
    for (int i = 0; i < kRuns; ++i) {
        const auto start = std::chrono::steady_clock::now();
        result = variant();
        times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(times.begin(), times.end());
    result.millis = times[kRuns / 2];
    return result;
}

void report(const char* title, const char* workName, const Result& a, const char* aName,
            const Result& b, const char* bName) {
    std::printf("\n%s\n", title);
    std::printf("  %-44s output %8zu   %s %9ld   median %8.1f ms\n", aName, a.output.size(), workName, a.work, a.millis);
    std::printf("  %-44s output %8zu   %s %9ld   median %8.1f ms\n", bName, b.output.size(), workName, b.work, b.millis);
    std::printf("  identical output: %s   time B/A: %.2f\n", a.output == b.output ? "YES" : "NO",
                b.millis / a.millis);
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string input = argc > 1 ? argv[1] : "data/access-small.log";
    const int copies = argc > 2 ? std::stoi(argv[2]) : 5000;

    // Prepare data: read and parse the sample once, then repeat it.
    std::vector<std::string> sampleLines;
    {
        PipelineBuilder::from(std::make_unique<FileLineSource>(input))
            .to(std::make_unique<LineCollector<std::string>>(sampleLines))
            .run();
    }
    std::vector<LogRecord> sampleRecords;
    {
        std::ostringstream ignored;
        ParserStage parser(ignored);
        struct Into : Emitter<LogRecord> {
            std::vector<LogRecord>& v;
            explicit Into(std::vector<LogRecord>& v) : v(v) {}
            void emit(const LogRecord& r) override { v.push_back(r); }
        } into(sampleRecords);
        for (const auto& line : sampleLines) parser.process(line, into);
    }
    std::vector<LogRecord> records;
    std::vector<std::string> lines;
    for (int i = 0; i < copies; ++i) {
        records.insert(records.end(), sampleRecords.begin(), sampleRecords.end());
        lines.insert(lines.end(), sampleLines.begin(), sampleLines.end());
    }
    std::printf("Data: %s x %d = %zu lines, %zu valid records. Median of %d runs.\n", input.c_str(), copies,
                lines.size(), records.size(), kRuns);

    const std::vector<std::string> fields = {"timestamp", "clientIp", "status", "path"};

    // ---- Experiment 1: order of two filters --------------------------------------
    const auto twoFilters = [&](bool statusFirst) {
        return [&, statusFirst] {
            Result r;
            auto status = counted<LogRecord>(predicates::minStatus(400), r.work);
            auto path = counted<LogRecord>(predicates::pathPrefix("/api"), r.work);
            PipelineBuilder::from(std::make_unique<VectorSource<LogRecord>>(records))
                .via(std::make_unique<PredicateFilterStage<>>(statusFirst ? status : path))
                .via(std::make_unique<PredicateFilterStage<>>(statusFirst ? path : status))
                .to(std::make_unique<LineCollector<LogRecord>>(r.output))
                .run();
            return r;
        };
    };
    report("Experiment 1: two filters, both orders (status >= 400, path starts with /api)",
           "predicate calls", measure(twoFilters(true)), "A: status filter -> path filter",
           measure(twoFilters(false)), "B: path filter -> status filter");

    // ---- Experiment 2: filter before or after projection -------------------------
    const auto filterThenProject = [&] {
        Result r;
        PipelineBuilder::from(std::make_unique<VectorSource<LogRecord>>(records))
            .via(std::make_unique<PredicateFilterStage<>>(predicates::minStatus(400)))
            .via(std::make_unique<ProjectionStage>(fields))
            .to(std::make_unique<LineCollector<ProjectedRecord>>(r.output))
            .run();
        r.work = static_cast<long>(r.output.size());  // projections = records that reach it
        return r;
    };
    const auto projectThenFilter = [&] {
        Result r;
        PipelineBuilder::from(std::make_unique<VectorSource<LogRecord>>(records))
            .via(std::make_unique<ProjectionStage>(fields))
            .via(std::make_unique<PredicateFilterStage<ProjectedRecord>>(
                [](const ProjectedRecord& p) { return std::stoi(*p.value("status")) >= 400; }))
            .to(std::make_unique<LineCollector<ProjectedRecord>>(r.output))
            .run();
        r.work = static_cast<long>(records.size());  // every record is projected
        return r;
    };
    report("Experiment 2: filter (status >= 400) before or after projection (4 fields)", "projections    ",
           measure(filterThenProject), "A: filter -> project", measure(projectThenFilter),
           "B: project -> filter (on projected text)");

    // ---- Experiment 3: the same as experiment 1, starting from raw text ----------
    // Parsing is slow, so this one uses a tenth of the lines.
    const std::vector<std::string> someLines(lines.begin(), lines.begin() + static_cast<long>(lines.size() / 10));
    const auto fromText = [&](bool statusFirst) {
        return [&, statusFirst] {
            Result r;
            std::ostringstream ignored;
            auto status = counted<LogRecord>(predicates::minStatus(400), r.work);
            auto path = counted<LogRecord>(predicates::pathPrefix("/api"), r.work);
            PipelineBuilder::from(std::make_unique<VectorSource<std::string>>(someLines))
                .via(std::make_unique<ParserStage>(ignored))
                .via(std::make_unique<PredicateFilterStage<>>(statusFirst ? status : path))
                .via(std::make_unique<PredicateFilterStage<>>(statusFirst ? path : status))
                .to(std::make_unique<LineCollector<LogRecord>>(r.output))
                .run();
            return r;
        };
    };
    std::printf("\n(Experiment 3 uses %zu lines.)", someLines.size());
    report("Experiment 3: experiment 1 including parsing of the raw lines", "predicate calls",
           measure(fromText(true)), "A: parse -> status -> path", measure(fromText(false)),
           "B: parse -> path -> status");

    // ---- Experiment 4: filtering before parsing ----------------------------------
    std::printf("\nExperiment 4: filter before parse\n");
    try {
        Pipeline pipeline(std::make_unique<VectorSource<std::string>>(lines));
        pipeline.addStage(std::make_unique<PredicateFilterStage<>>(predicates::minStatus(400)));
        std::printf("  unexpectedly accepted\n");
    } catch (const std::logic_error& e) {
        std::printf("  rejected while assembling: %s\n", e.what());
    }
    std::printf("  (with PipelineBuilder the same mistake does not even compile)\n");
    return 0;
}
