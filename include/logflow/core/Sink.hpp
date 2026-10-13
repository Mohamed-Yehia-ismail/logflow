#pragma once

namespace logflow {

// The end of a pipeline: receives every item that reaches it.
template <typename I>
class Sink {
public:
    using InputType = I;

    virtual ~Sink() = default;
    virtual void consume(const I& item) = 0;

    // Lifecycle hook called once before the first item (for example, to open a file).
    virtual void open() {}

    // Lifecycle hook called once after the last item (for example, to flush and close
    // a file). A sink that holds a resource must release it here.
    virtual void close() {}
};

}  // namespace logflow
