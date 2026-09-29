#pragma once

namespace logflow {

// The connector between two components: whatever is emitted here is handed
// to the next component downstream.
template <typename T>
class Emitter {
public:
    virtual ~Emitter() = default;
    virtual void emit(const T& item) = 0;
};

}  // namespace logflow
