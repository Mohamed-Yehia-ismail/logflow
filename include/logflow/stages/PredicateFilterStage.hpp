#pragma once

#include <functional>
#include <stdexcept>
#include <utility>

#include "logflow/core/Stage.hpp"
#include "logflow/model/LogRecord.hpp"

namespace logflow {

// Passes an item on only if the predicate accepts it; otherwise drops it.
// Works for any item type, LogRecord by default.
template <typename T = LogRecord>
class PredicateFilterStage : public Stage<T, T> {
public:
    using Predicate = std::function<bool(const T&)>;

    explicit PredicateFilterStage(Predicate predicate) : predicate_(std::move(predicate)) {
        if (!predicate_) throw std::invalid_argument("PredicateFilterStage: predicate is empty");
    }

    void process(const T& item, Emitter<T>& out) override {
        if (predicate_(item)) out.emit(item);
    }

private:
    Predicate predicate_;
};

}  // namespace logflow
