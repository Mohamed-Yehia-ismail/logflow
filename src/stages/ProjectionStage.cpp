#include "logflow/stages/ProjectionStage.hpp"

#include <stdexcept>
#include <utility>

#include "logflow/model/LogRecordFields.hpp"

namespace logflow {

ProjectionStage::ProjectionStage(std::vector<std::string> fields) : fields_(std::move(fields)) {
    if (fields_.empty()) throw std::invalid_argument("ProjectionStage: no fields selected");
    const LogRecord probe = LogRecord::Builder().build();
    for (const std::string& name : fields_) {
        if (!logRecordField(probe, name)) {
            throw std::invalid_argument("ProjectionStage: unknown field '" + name + "'");
        }
    }
}

void ProjectionStage::process(const LogRecord& record, Emitter<ProjectedRecord>& out) {
    std::vector<ProjectedRecord::Field> selected;
    selected.reserve(fields_.size());
    for (const std::string& name : fields_) selected.emplace_back(name, *logRecordField(record, name));
    out.emit(ProjectedRecord(std::move(selected)));
}

}  // namespace logflow
