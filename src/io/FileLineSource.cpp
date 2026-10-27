#include "logflow/io/FileLineSource.hpp"

#include <fstream>
#include <utility>

#include "logflow/core/PipelineException.hpp"

namespace logflow {

FileLineSource::FileLineSource(std::string path) : path_(std::move(path)) {}

void FileLineSource::produce(Emitter<std::string>& out) {
    std::ifstream in(path_);
    if (!in) throw PipelineException("Cannot read " + path_);

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF files
        out.emit(line);
    }
    if (in.bad()) throw PipelineException("Error while reading " + path_);
}

}  // namespace logflow
