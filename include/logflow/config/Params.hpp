#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace logflow {

// One parameter value from a configuration file: a single value or a list of values.
struct ParamValue {
    std::optional<std::string> scalar;
    std::optional<std::vector<std::string>> list;
    int line = 0;
};

// The parameters of one configured component, with typed and validated access.
//
// Every accessor reports problems as a ConfigError that names the file, the line,
// the component and the parameter, so the user knows exactly what to fix.
class Params {
public:
    // `file` and `line` locate the component; `component` describes it, e.g. "stage 2 (filter)".
    Params(std::string file, int line, std::string component, std::map<std::string, ParamValue> values = {});

    // Rejects any parameter that is not in `names`, and remembers the names for messages.
    void allowOnly(const std::vector<std::string>& names) const;

    std::string requireString(const std::string& key) const;
    std::optional<std::string> optionalString(const std::string& key) const;
    std::vector<std::string> requireList(const std::string& key) const;

    // Throws a ConfigError located at `key` (or at the component if `key` is empty).
    [[noreturn]] void fail(const std::string& key, const std::string& message) const;

private:
    const ParamValue& require(const std::string& key) const;
    std::string expectedList() const;

    std::string file_;
    int line_;
    std::string component_;
    std::map<std::string, ParamValue> values_;
    mutable std::vector<std::string> known_;
};

}  // namespace logflow
