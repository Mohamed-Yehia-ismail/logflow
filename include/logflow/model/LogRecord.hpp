#pragma once

#include <map>
#include <optional>
#include <string>

#include "logflow/core/Record.hpp"
#include "logflow/util/Timestamp.hpp"

namespace logflow {

// One parsed access-log entry: the domain model that flows through the pipeline.
//
// LogRecord is immutable. It is created with LogRecord::Builder and has only const
// accessors. "Changing" a record means creating a new one (see withAttribute).
//
// attributes is the extension point of the record format. Later stages will attach
// data that today's parser knows nothing about (for example a geo location or a
// classification). Adding it as an open key/value map now means those stages can
// enrich records without changing LogRecord, the parser, or any other stage.
class LogRecord : public Record {
public:
    using Attributes = std::map<std::string, std::string>;

    class Builder;

    Timestamp timestamp() const { return timestamp_; }
    const std::string& clientIp() const { return clientIp_; }
    const std::string& method() const { return method_; }
    const std::string& path() const { return path_; }
    int status() const { return status_; }
    long long bytes() const { return bytes_; }
    const std::string& userAgent() const { return userAgent_; }
    const Attributes& attributes() const { return attributes_; }
    const std::string& raw() const override { return raw_; }

    // The value of one attribute, or std::nullopt if it is not set.
    std::optional<std::string> attribute(const std::string& key) const;

    // A copy of this record with one attribute added or replaced.
    LogRecord withAttribute(const std::string& key, const std::string& value) const;

private:
    LogRecord() = default;

    Timestamp timestamp_{};
    std::string clientIp_;
    std::string method_;
    std::string path_;
    int status_ = 0;
    long long bytes_ = 0;
    std::string userAgent_;
    Attributes attributes_;
    std::string raw_;
};

// Collects the fields of a LogRecord, then builds it in one step.
class LogRecord::Builder {
public:
    Builder& timestamp(Timestamp value) { record_.timestamp_ = value; return *this; }
    Builder& clientIp(std::string value) { record_.clientIp_ = std::move(value); return *this; }
    Builder& method(std::string value) { record_.method_ = std::move(value); return *this; }
    Builder& path(std::string value) { record_.path_ = std::move(value); return *this; }
    Builder& status(int value) { record_.status_ = value; return *this; }
    Builder& bytes(long long value) { record_.bytes_ = value; return *this; }
    Builder& userAgent(std::string value) { record_.userAgent_ = std::move(value); return *this; }
    Builder& raw(std::string value) { record_.raw_ = std::move(value); return *this; }
    Builder& attribute(const std::string& key, std::string value) {
        record_.attributes_[key] = std::move(value);
        return *this;
    }

    LogRecord build() const { return record_; }

private:
    LogRecord record_;
};

}  // namespace logflow
