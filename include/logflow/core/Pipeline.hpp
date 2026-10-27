#pragma once

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "logflow/core/Emitter.hpp"
#include "logflow/core/ErrorPolicy.hpp"
#include "logflow/core/FailedRecord.hpp"
#include "logflow/core/Record.hpp"
#include "logflow/core/RunSummary.hpp"
#include "logflow/core/Sink.hpp"
#include "logflow/core/Source.hpp"
#include "logflow/core/Stage.hpp"

namespace logflow {

// How a stage is shown in the run summary and what happens when it fails on an item.
struct StageOptions {
    std::string name;                           // empty: "stage N"
    ErrorPolicy onError = ErrorPolicy::skip();
};

// Connects one Source, an ordered list of Stages and one Sink, and pushes every
// item from the source through the stages to the sink.
//
// Stages may change the item type (for example std::string -> a parsed record),
// so items travel between components as std::any. Types are checked while the
// pipeline is assembled: a stage whose input type does not match the previous
// output type is rejected immediately with std::logic_error.
//
// Error handling. When a stage throws StageException for an item, the pipeline
// applies that stage's ErrorPolicy. Under DEAD_LETTER (and RETRY(n), once the
// retries are used up) the item leaves the main line as a FailedRecord and goes to
// a second, independent sink: the dead-letter sink. So the pipeline is no longer a
// single straight line:
//
//   source ──> stage 1 ──> stage 2 ──> ... ──> sink
//                 │           │
//                 └───────────┴──> dead-letter sink   (FailedRecord)
//
// Any other exception (PipelineException, or an unexpected error) stops the run.
//
// With no stages, items go straight from the source to the sink.
class Pipeline {
public:
    // Each component is any class derived from Source, Stage or Sink; its item
    // types are read from the OutputType / InputType aliases of the interface.
    template <typename S>
    explicit Pipeline(std::unique_ptr<S> source);

    template <typename S>
    Pipeline& addStage(std::unique_ptr<S> stage, StageOptions options = {});

    template <typename S>
    Pipeline& setSink(std::unique_ptr<S> sink);

    // The sink for items that failed under DEAD_LETTER or RETRY(n).
    template <typename S>
    Pipeline& setDeadLetterSink(std::unique_ptr<S> sink);

    // The name shown in the run summary.
    void setName(std::string name) { name_ = std::move(name); }
    const std::string& name() const { return name_; }

    // Runs the pipeline until the source is exhausted and returns the counts.
    // Throws PipelineException if the run cannot continue.
    RunSummary run();

    // The counts of the last run, also when it stopped with an exception.
    const RunSummary& lastSummary() const { return summary_; }

private:
    using Next = std::function<void(const std::any&)>;

    struct ErasedStage {
        std::function<void(const std::any&, const Next&)> process;
        std::function<void()> open;
        std::function<void()> close;
        std::function<std::string(const std::any&)> raw;
        StageOptions options;
    };

    struct ErasedSink {
        Next consume;
        std::function<void()> open;
        std::function<void()> close;
    };

    // Adapts a typed Emitter<T> onto the type-erased chain.
    template <typename T>
    class ErasedEmitter : public Emitter<T> {
    public:
        explicit ErasedEmitter(const Next& next) : next_(next) {}
        void emit(const T& item) override { next_(std::any(item)); }

    private:
        const Next& next_;
    };

    // The original text of an item, for the dead-letter channel.
    template <typename T>
    static std::string rawTextOf(const T& item) {
        if constexpr (std::is_same_v<T, std::string>) {
            return item;
        } else if constexpr (std::is_base_of_v<Record, T>) {
            return item.raw();
        } else {
            return "";
        }
    }

    template <typename T>
    void expectType(const char* component) const;

    // Runs stage `index` on one item and applies its error policy.
    void process(std::size_t index, const std::any& item, const Next& next);

    template <typename S>
    static ErasedSink eraseSink(std::unique_ptr<S> sink);

    std::function<void(const Next&)> produce_;
    std::vector<ErasedStage> stages_;
    ErasedSink sink_;
    ErasedSink deadLetter_;
    std::type_index currentType_;
    std::string name_;
    RunSummary summary_;
};

// ---- template implementation ----

template <typename S>
Pipeline::Pipeline(std::unique_ptr<S> source) : currentType_(typeid(typename S::OutputType)) {
    using O = typename S::OutputType;
    if (!source) throw std::invalid_argument("Pipeline: source is null");
    std::shared_ptr<Source<O>> owned = std::move(source);
    produce_ = [owned](const Next& next) {
        ErasedEmitter<O> out(next);
        owned->produce(out);
    };
}

template <typename S>
Pipeline& Pipeline::addStage(std::unique_ptr<S> stage, StageOptions options) {
    using I = typename S::InputType;
    using O = typename S::OutputType;
    if (!stage) throw std::invalid_argument("Pipeline: stage is null");
    expectType<I>("stage");
    if (options.name.empty()) options.name = "stage " + std::to_string(stages_.size() + 1);
    std::shared_ptr<Stage<I, O>> owned = std::move(stage);
    stages_.push_back(ErasedStage{
        [owned](const std::any& item, const Next& next) {
            ErasedEmitter<O> out(next);
            owned->process(std::any_cast<const I&>(item), out);
        },
        [owned] { owned->open(); },
        [owned] { owned->close(); },
        [](const std::any& item) { return rawTextOf(std::any_cast<const I&>(item)); },
        std::move(options),
    });
    currentType_ = typeid(O);
    return *this;
}

template <typename S>
Pipeline::ErasedSink Pipeline::eraseSink(std::unique_ptr<S> sink) {
    using I = typename S::InputType;
    std::shared_ptr<Sink<I>> owned = std::move(sink);
    return ErasedSink{
        [owned](const std::any& item) { owned->consume(std::any_cast<const I&>(item)); },
        [owned] { owned->open(); },
        [owned] { owned->close(); },
    };
}

template <typename S>
Pipeline& Pipeline::setSink(std::unique_ptr<S> sink) {
    if (!sink) throw std::invalid_argument("Pipeline: sink is null");
    expectType<typename S::InputType>("sink");
    sink_ = eraseSink(std::move(sink));
    return *this;
}

template <typename S>
Pipeline& Pipeline::setDeadLetterSink(std::unique_ptr<S> sink) {
    static_assert(std::is_same_v<typename S::InputType, FailedRecord>,
                  "a dead-letter sink must accept FailedRecord");
    if (!sink) throw std::invalid_argument("Pipeline: dead-letter sink is null");
    deadLetter_ = eraseSink(std::move(sink));
    return *this;
}

template <typename T>
void Pipeline::expectType(const char* component) const {
    if (currentType_ != std::type_index(typeid(T))) {
        throw std::logic_error(std::string("Pipeline: ") + component +
                               " input type does not match the previous output type");
    }
}

}  // namespace logflow
