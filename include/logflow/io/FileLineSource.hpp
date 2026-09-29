#pragma once

#include <string>

#include "logflow/core/Source.hpp"

namespace logflow {

// Reads a text file and emits one std::string per line.
class FileLineSource : public Source<std::string> {
public:
    explicit FileLineSource(std::string path);

    void produce(Emitter<std::string>& out) override;

private:
    std::string path_;
};

}  // namespace logflow
