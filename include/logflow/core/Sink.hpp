#pragma once

namespace logflow {

// The end of a pipeline: receives every item that reaches it.
template <typename I>
class Sink {
public:
    using InputType = I;

    virtual ~Sink() = default;
    virtual void consume(const I& item) = 0;
};

}  // namespace logflow
