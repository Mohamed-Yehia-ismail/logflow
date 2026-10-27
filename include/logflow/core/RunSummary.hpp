#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace logflow {

// What happened to the items in one stage during a run.
struct StageStats {
    std::string name;
    std::string policy;            // the error policy, e.g. "DEAD_LETTER"
    std::size_t in = 0;            // items received
    std::size_t out = 0;           // items emitted
    std::size_t skipped = 0;       // failed, dropped under SKIP
    std::size_t deadLettered = 0;  // failed, sent to the dead-letter sink
    std::size_t retries = 0;       // extra attempts made under RETRY(n)

    // Items the stage dropped on purpose (a filter, a comment line), without an error.
    // Assumes the stage emits at most one item per input, as all built-in stages do.
    std::size_t filtered() const {
        const std::size_t accounted = out + skipped + deadLettered;
        return in > accounted ? in - accounted : 0;
    }
};

// The counts of one pipeline run. Printed at the end of every run.
//
// It is plain data, kept apart from how it is printed, so that later increments can
// collect, compare or export these numbers as metrics.
struct RunSummary {
    std::string pipelineName;
    std::size_t recordsRead = 0;      // items produced by the source
    std::size_t recordsWritten = 0;   // items that reached the main sink
    std::vector<StageStats> stages;
    std::chrono::nanoseconds elapsed{0};
    bool completed = false;           // false if the run stopped early

    std::size_t deadLettered() const;
    std::size_t dropped() const;      // filtered + skipped, over all stages

    // True if every record read is accounted for:
    //   read = written + dropped + dead-lettered
    bool balanced() const;
};

// The summary as a short table, ready to print.
std::string formatSummary(const RunSummary& summary);

}  // namespace logflow
