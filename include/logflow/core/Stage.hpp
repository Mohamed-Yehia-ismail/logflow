#pragma once

#include "logflow/core/Emitter.hpp"
#include "logflow/core/StageException.hpp"

namespace logflow {

// A processing step between a Source and a Sink.
//
// process() emits through an Emitter instead of returning a value because a
// stage may produce zero, one, or many outputs per input (a filter drops items,
// a splitter multiplies them). A return value would force a 1:1 mapping.
//
// A stage that fails to process an item throws StageException.
template <typename I, typename O>
class Stage {
public:
    using InputType = I;
    using OutputType = O;

    virtual ~Stage() = default;

    virtual void process(const I& input, Emitter<O>& out) = 0;

    // Lifecycle hook called once before the first item. Unused in increment 1.
    virtual void open() {}

    // Lifecycle hook called once after the last item. Unused in increment 1.
    virtual void close() {}
};

}  // namespace logflow
