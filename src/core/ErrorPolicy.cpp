#include "logflow/core/ErrorPolicy.hpp"

#include <cctype>

namespace logflow {

std::optional<ErrorPolicy> ErrorPolicy::parse(const std::string& text) {
    std::string upper;
    for (char c : text) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper == "FAIL_FAST") return failFast();
    if (upper == "SKIP") return skip();
    if (upper == "DEAD_LETTER") return deadLetter();

    // RETRY(n), 1 <= n <= 10
    const std::string prefix = "RETRY(";
    if (upper.size() < prefix.size() + 2 || upper.compare(0, prefix.size(), prefix) != 0 || upper.back() != ')') {
        return std::nullopt;
    }
    const std::string digits = upper.substr(prefix.size(), upper.size() - prefix.size() - 1);
    if (digits.empty() || digits.size() > 2) return std::nullopt;
    for (char c : digits) {
        if (c < '0' || c > '9') return std::nullopt;
    }
    const int times = std::stoi(digits);
    if (times < 1 || times > 10) return std::nullopt;
    return retry(times);
}

std::string ErrorPolicy::name() const {
    switch (kind) {
        case Kind::FailFast: return "FAIL_FAST";
        case Kind::Skip: return "SKIP";
        case Kind::DeadLetter: return "DEAD_LETTER";
        case Kind::Retry: return "RETRY(" + std::to_string(retries) + ")";
    }
    return "";
}

}  // namespace logflow
