#include "logflow/core/Pipeline.hpp"

namespace logflow {

void Pipeline::run() {
    if (!consume_) throw std::logic_error("Pipeline: no sink set");

    // Build the chain back to front: the last link feeds the sink, each earlier
    // link feeds its stage, which emits into the next link.
    std::vector<Next> links(stages_.size() + 1);
    links.back() = consume_;
    for (std::size_t i = stages_.size(); i-- > 0;) {
        const ErasedStage& stage = stages_[i];
        const Next& next = links[i + 1];
        links[i] = [&stage, &next](const std::any& item) { stage.process(item, next); };
    }

    // Open stages in order; close the opened ones in reverse, even on failure.
    std::size_t opened = 0;
    try {
        for (; opened < stages_.size(); ++opened) stages_[opened].open();
        produce_(links.front());
    } catch (...) {
        while (opened > 0) stages_[--opened].close();
        throw;
    }
    while (opened > 0) stages_[--opened].close();
}

}  // namespace logflow
