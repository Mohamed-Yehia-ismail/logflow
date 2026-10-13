#pragma once

#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "logflow/core/Sink.hpp"
#include "logflow/io/LineFormat.hpp"
#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Writes every item it receives as one line to a file (see LineFormat.hpp).
//
// open() creates or truncates the file; close() flushes and closes it. Output is
// buffered, so until close() runs, the last lines exist only in memory: a sink
// that is never closed loses them.
template <typename T = LogRecord>
class FileSink : public Sink<T> {
public:
    static constexpr std::size_t kBufferSize = 64 * 1024;

    explicit FileSink(std::string path) : path_(std::move(path)) {}

    void open() override {
        file_.rdbuf()->pubsetbuf(buffer_.get(), kBufferSize);  // must precede open
        file_.open(path_, std::ios::out | std::ios::trunc);
        if (!file_) throw std::runtime_error("Cannot open output file " + path_);
    }

    void consume(const T& item) override {
        if (!file_.is_open()) throw std::logic_error("FileSink: consume() before open()");
        file_ << formatLine(item) << '\n';
    }

    void close() override {
        if (!file_.is_open()) return;
        file_.close();  // flushes the buffer
        if (file_.fail()) throw std::runtime_error("Error while writing " + path_);
    }

private:
    std::string path_;
    std::unique_ptr<char[]> buffer_ = std::make_unique<char[]>(kBufferSize);
    std::ofstream file_;
};

}  // namespace logflow
