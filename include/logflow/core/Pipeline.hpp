#pragma once

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "logflow/core/Emitter.hpp"
#include "logflow/core/Sink.hpp"
#include "logflow/core/Source.hpp"
#include "logflow/core/Stage.hpp"

namespace logflow {

// Connects one Source, an ordered list of Stages and one Sink, and pushes every
// item from the source through the stages to the sink.
//
// Stages may change the item type (for example std::string -> a parsed record),
// so items travel between components as std::any. Types are checked while the
// pipeline is assembled: a stage whose input type does not match the previous
// output type is rejected immediately with std::logic_error.
//
// With no stages, items go straight from the source to the sink.
class Pipeline {
public:
    // Each component is any class derived from Source, Stage or Sink; its item
    // types are read from the OutputType / InputType aliases of the interface.
    template <typename S>
    explicit Pipeline(std::unique_ptr<S> source);

    template <typename S>
    Pipeline& addStage(std::unique_ptr<S> stage);

    template <typename S>
    Pipeline& setSink(std::unique_ptr<S> sink);

    // Runs the pipeline until the source is exhausted.
    void run();

private:
    using Next = std::function<void(const std::any&)>;

    struct ErasedStage {
        std::function<void(const std::any&, const Next&)> process;
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

    template <typename T>
    void expectType(const char* component) const;

    std::function<void(const Next&)> produce_;
    std::vector<ErasedStage> stages_;
    Next consume_;
    std::type_index currentType_;
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
Pipeline& Pipeline::addStage(std::unique_ptr<S> stage) {
    using I = typename S::InputType;
    using O = typename S::OutputType;
    if (!stage) throw std::invalid_argument("Pipeline: stage is null");
    expectType<I>("stage");
    std::shared_ptr<Stage<I, O>> owned = std::move(stage);
    stages_.push_back(ErasedStage{
        [owned](const std::any& item, const Next& next) {
            ErasedEmitter<O> out(next);
            owned->process(std::any_cast<const I&>(item), out);
        },
        [owned] { owned->open(); },
        [owned] { owned->close(); },
    });
    currentType_ = typeid(O);
    return *this;
}

template <typename S>
Pipeline& Pipeline::setSink(std::unique_ptr<S> sink) {
    using I = typename S::InputType;
    if (!sink) throw std::invalid_argument("Pipeline: sink is null");
    expectType<I>("sink");
    std::shared_ptr<Sink<I>> owned = std::move(sink);
    consume_ = [owned](const std::any& item) { owned->consume(std::any_cast<const I&>(item)); };
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
