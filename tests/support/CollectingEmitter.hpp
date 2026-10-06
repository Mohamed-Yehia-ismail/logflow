#pragma once

#include <vector>

#include "logflow/core/Emitter.hpp"

namespace logflow::test {

// Test double: an Emitter that stores everything emitted into it.
//
// A stage test builds the stage, passes it an input and this emitter, then checks
// `items`. No file, console or pipeline is involved, so the stage is tested in
// complete isolation.
template <typename T>
class CollectingEmitter : public Emitter<T> {
public:
    void emit(const T& item) override { items.push_back(item); }

    std::vector<T> items;
};

}  // namespace logflow::test
