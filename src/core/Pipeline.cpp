#include "logflow/core/Pipeline.hpp"

namespace logflow {

void Pipeline::run() {
    if (!sink_.consume) throw std::logic_error("Pipeline: no sink set");

    // Build the chain back to front: the last link feeds the sink, each earlier
    // link feeds its stage, which emits into the next link.
    std::vector<Next> links(stages_.size() + 1);
    links.back() = sink_.consume;
    for (std::size_t i = stages_.size(); i-- > 0;) {
        const ErasedStage& stage = stages_[i];
        const Next& next = links[i + 1];
        links[i] = [&stage, &next](const std::any& item) { stage.process(item, next); };
    }

    // The sink is opened first and closed last, because a stage may still emit
    // items while it is being closed. Stages are opened in order and closed in
    // reverse. Everything that was opened is closed, even when the run fails.
    sink_.open();
    std::size_t opened = 0;
    try {
        for (; opened < stages_.size(); ++opened) stages_[opened].open();
        produce_(links.front());
        while (opened > 0) stages_[--opened].close();
    } catch (...) {
        // Keep the original error: failures while cleaning up are ignored.
        while (opened > 0) {
            try { stages_[--opened].close(); } catch (...) {}
        }
        try { sink_.close(); } catch (...) {}
        throw;
    }
    sink_.close();
}

}  // namespace logflow
