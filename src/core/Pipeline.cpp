#include "logflow/core/Pipeline.hpp"

#include <chrono>

#include "logflow/core/PipelineException.hpp"

namespace logflow {

RunSummary Pipeline::run() {
    if (!sink_.consume) throw std::logic_error("Pipeline: no sink set");
    for (const ErasedStage& stage : stages_) {
        if (stage.options.onError.usesDeadLetter() && !deadLetter_.consume) {
            throw std::logic_error("Pipeline: stage '" + stage.options.name + "' uses " +
                                   stage.options.onError.name() + " but no dead-letter sink is set");
        }
    }

    summary_ = RunSummary{};
    summary_.pipelineName = name_;
    for (const ErasedStage& stage : stages_) {
        StageStats stats;
        stats.name = stage.options.name;
        stats.policy = stage.options.onError.name();
        summary_.stages.push_back(stats);
    }
    const auto started = std::chrono::steady_clock::now();
    const auto stopClock = [&] { summary_.elapsed = std::chrono::steady_clock::now() - started; };

    // Build the chain back to front: the last link feeds the sink, each earlier
    // link feeds its stage, which emits into the next link.
    std::vector<Next> links(stages_.size() + 1);
    links.back() = [this](const std::any& item) {
        sink_.consume(item);
        ++summary_.recordsWritten;
    };
    for (std::size_t i = stages_.size(); i-- > 0;) {
        const Next& next = links[i + 1];
        links[i] = [this, i, &next](const std::any& item) { process(i, item, next); };
    }
    const Next first = [this, &links](const std::any& item) {
        ++summary_.recordsRead;
        links.front()(item);
    };

    // The sinks are opened first and closed last, because a stage may still emit
    // items while it is being closed. Stages are opened in order and closed in
    // reverse. Everything that was opened is closed, even when the run fails.
    const bool hasDeadLetter = static_cast<bool>(deadLetter_.consume);
    sink_.open();
    std::size_t opened = 0;
    bool deadLetterOpen = false;
    try {
        if (hasDeadLetter) {
            deadLetter_.open();
            deadLetterOpen = true;
        }
        for (; opened < stages_.size(); ++opened) stages_[opened].open();
        produce_(first);
        while (opened > 0) stages_[--opened].close();
        deadLetterOpen = false;
        if (hasDeadLetter) deadLetter_.close();
    } catch (...) {
        // Keep the original error: failures while cleaning up are ignored.
        stopClock();
        while (opened > 0) {
            try { stages_[--opened].close(); } catch (...) {}
        }
        if (deadLetterOpen) {
            try { deadLetter_.close(); } catch (...) {}
        }
        try { sink_.close(); } catch (...) {}
        throw;
    }
    sink_.close();
    stopClock();
    summary_.completed = true;
    return summary_;
}

void Pipeline::process(std::size_t index, const std::any& item, const Next& next) {
    const ErasedStage& stage = stages_[index];
    const ErrorPolicy& policy = stage.options.onError;
    StageStats& stats = summary_.stages[index];
    ++stats.in;

    const Next counted = [&stats, &next](const std::any& output) {
        ++stats.out;
        next(output);
    };

    // Only StageException is handled here. A StageException thrown further down the
    // line has already been handled by the stage that threw it, so what arrives here
    // comes from this stage. Anything else stops the run.
    for (int attempt = 0;; ++attempt) {
        try {
            stage.process(item, counted);
            return;
        } catch (const StageException& e) {
            if (policy.kind == ErrorPolicy::Kind::Retry && attempt < policy.retries) {
                ++stats.retries;
                continue;
            }
            switch (policy.kind) {
                case ErrorPolicy::Kind::FailFast:
                    throw PipelineException("stage " + std::to_string(index + 1) + " (" + stage.options.name +
                                            ") failed on record " + std::to_string(summary_.recordsRead) +
                                            " under FAIL_FAST: " + e.what());
                case ErrorPolicy::Kind::Skip:
                    ++stats.skipped;
                    return;
                case ErrorPolicy::Kind::DeadLetter:
                case ErrorPolicy::Kind::Retry:
                    ++stats.deadLettered;
                    deadLetter_.consume(std::any(FailedRecord{stage.raw(item), stage.options.name, e.what(),
                                                              std::chrono::system_clock::now(),
                                                              summary_.recordsRead}));
                    return;
            }
        }
    }
}

}  // namespace logflow
