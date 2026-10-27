// Entry point. Reads the arguments, assembles the pipeline, runs it and prints the
// run summary. No business logic lives here.

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "logflow/app/CommandLine.hpp"
#include "logflow/app/PipelineAssembly.hpp"
#include "logflow/config/ConfigError.hpp"

int main(int argc, char* argv[]) {
    std::optional<logflow::Pipeline> pipeline;
    try {
        const logflow::Options options =
            logflow::parseCommandLine(std::vector<std::string>(argv + 1, argv + argc));
        if (options.help) {
            std::cout << logflow::usageText();
            return 0;
        }

        pipeline.emplace(logflow::assemblePipeline(options));

        std::cerr << logflow::formatSummary(pipeline->run());
    } catch (const logflow::UsageError& e) {
        std::cerr << "logflow: " << e.what() << "\n\n" << logflow::usageText();
        return 2;
    } catch (const logflow::ConfigError& e) {
        std::cerr << "logflow: configuration error: " << e.what() << '\n';
        return 3;
    } catch (const std::exception& e) {
        // The run stopped: show how far it got.
        if (pipeline) std::cerr << logflow::formatSummary(pipeline->lastSummary());
        std::cerr << "logflow: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
