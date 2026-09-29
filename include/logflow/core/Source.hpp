#pragma once

#include "logflow/core/Emitter.hpp"

namespace logflow {

// The start of a pipeline: produces items and pushes them downstream.
template <typename O>
class Source {
public:
    using OutputType = O;

    virtual ~Source() = default;
    virtual void produce(Emitter<O>& out) = 0;
};

}  // namespace logflow
