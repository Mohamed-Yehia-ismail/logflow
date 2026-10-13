#pragma once

#include <string>
#include <vector>

#include "logflow/core/Stage.hpp"
#include "logflow/model/LogRecord.hpp"
#include "logflow/model/ProjectedRecord.hpp"

namespace logflow {

// Reduces every record to the selected fields, in the given order.
//
// Data minimization: downstream components (and the people who read the output)
// receive only the fields they need. The raw line, which contains everything, is
// never passed on.
class ProjectionStage : public Stage<LogRecord, ProjectedRecord> {
public:
    // Throws std::invalid_argument if the list is empty or contains an unknown field.
    explicit ProjectionStage(std::vector<std::string> fields);

    void process(const LogRecord& record, Emitter<ProjectedRecord>& out) override;

private:
    std::vector<std::string> fields_;
};

}  // namespace logflow
