#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace logflow {

// A record reduced to a chosen subset of fields, in the chosen order.
//
// Produced by ProjectionStage. Values are already rendered as text, because a
// projection is meant for output: it carries only what the reader asked for.
// Like LogRecord it is immutable.
class ProjectedRecord {
public:
    using Field = std::pair<std::string, std::string>;  // name, value

    explicit ProjectedRecord(std::vector<Field> fields) : fields_(std::move(fields)) {}

    const std::vector<Field>& fields() const { return fields_; }

    // The value of one field, or std::nullopt if it was not selected.
    std::optional<std::string> value(const std::string& name) const {
        for (const Field& field : fields_) {
            if (field.first == name) return field.second;
        }
        return std::nullopt;
    }

private:
    std::vector<Field> fields_;
};

}  // namespace logflow
