#include "logflow/core/RunSummary.hpp"

#include <algorithm>
#include <cstdio>

namespace logflow {
namespace {

std::string padded(const std::string& text, std::size_t width) {
    return text.size() >= width ? text : text + std::string(width - text.size(), ' ');
}

std::string number(std::size_t value, int width) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%*zu", width, value);
    return buffer;
}

}  // namespace

std::size_t RunSummary::deadLettered() const {
    std::size_t total = 0;
    for (const StageStats& stage : stages) total += stage.deadLettered;
    return total;
}

std::size_t RunSummary::dropped() const {
    std::size_t total = 0;
    for (const StageStats& stage : stages) total += stage.filtered() + stage.skipped;
    return total;
}

bool RunSummary::balanced() const {
    return recordsRead == recordsWritten + dropped() + deadLettered();
}

std::string formatSummary(const RunSummary& s) {
    std::string text = "Run summary: " + (s.pipelineName.empty() ? std::string("pipeline") : s.pipelineName) +
                       (s.completed ? "" : " (stopped early)") + "\n";
    text += "  records read      " + number(s.recordsRead, 7) + "\n";

    std::size_t nameWidth = 5;
    for (const StageStats& stage : s.stages) nameWidth = std::max(nameWidth, stage.name.size());
    if (!s.stages.empty()) {
        text += "  " + padded("stage", nameWidth + 4) + padded("policy", 13) +
                "     in    out  filtered  skipped  dead-letter\n";
    }
    for (std::size_t i = 0; i < s.stages.size(); ++i) {
        const StageStats& st = s.stages[i];
        text += "  " + padded(std::to_string(i + 1) + ". " + st.name, nameWidth + 4) + padded(st.policy, 13) +
                number(st.in, 6) + number(st.out, 7) + number(st.filtered(), 10) + number(st.skipped, 9) +
                number(st.deadLettered, 13);
        if (st.retries > 0) text += "  (retries: " + std::to_string(st.retries) + ")";
        text += "\n";
    }

    text += "  records written   " + number(s.recordsWritten, 7) + "\n";
    text += "  dead-lettered     " + number(s.deadLettered(), 7) + "\n";

    char elapsed[32];
    std::snprintf(elapsed, sizeof elapsed, "%.3f ms", std::chrono::duration<double, std::milli>(s.elapsed).count());
    text += "  elapsed           " + std::string(elapsed) + "\n";

    if (s.completed) {
        text += "  check: " + std::to_string(s.recordsRead) + " read = " + std::to_string(s.recordsWritten) +
                " written + " + std::to_string(s.dropped()) + " dropped + " + std::to_string(s.deadLettered()) +
                " dead-lettered  " + (s.balanced() ? "OK" : "MISMATCH") + "\n";
    }
    return text;
}

}  // namespace logflow
