#pragma once

#include <optional>
#include <string>

namespace logflow {

// What the pipeline does when a stage throws StageException for one item.
// Chosen per stage (in a configuration file: onError).
//
//   FAIL_FAST    stop the run (PipelineException)
//   SKIP         drop the item and count it
//   DEAD_LETTER  send the item, with the reason, to the dead-letter sink
//   RETRY(n)     try the item again up to n more times; if it still fails, dead-letter it
//                (meant for I/O stages, whose failures may be temporary)
struct ErrorPolicy {
    enum class Kind { FailFast, Skip, DeadLetter, Retry };

    Kind kind = Kind::Skip;
    int retries = 0;

    static ErrorPolicy failFast() { return {Kind::FailFast, 0}; }
    static ErrorPolicy skip() { return {Kind::Skip, 0}; }
    static ErrorPolicy deadLetter() { return {Kind::DeadLetter, 0}; }
    static ErrorPolicy retry(int times) { return {Kind::Retry, times}; }

    // Reads FAIL_FAST, SKIP, DEAD_LETTER or RETRY(n), in upper or lower case.
    // Returns std::nullopt for anything else.
    static std::optional<ErrorPolicy> parse(const std::string& text);

    // The policy as written in a configuration file, for example "RETRY(3)".
    std::string name() const;

    // True if failed items may be sent to the dead-letter sink.
    bool usesDeadLetter() const { return kind == Kind::DeadLetter || kind == Kind::Retry; }
};

}  // namespace logflow
