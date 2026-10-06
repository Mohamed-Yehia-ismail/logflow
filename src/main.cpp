// Entry point. Reads the arguments, assembles the pipeline, runs it.
// No business logic lives here.

#include <exception>
#include <iostream>
#include <memory>

#include "logflow/core/Pipeline.hpp"
#include "logflow/io/ConsoleSink.hpp"
#include "logflow/io/FileLineSource.hpp"
#include "logflow/stages/ParserStage.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: logflow <log-file>\n";
        return 2;
    }

    try {
        logflow::Pipeline pipeline(std::make_unique<logflow::FileLineSource>(argv[1]));
        pipeline.addStage(std::make_unique<logflow::ParserStage>())
            .setSink(std::make_unique<logflow::ConsoleSink>());

        pipeline.run();
    } catch (const std::exception& e) {
        std::cerr << "logflow: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
