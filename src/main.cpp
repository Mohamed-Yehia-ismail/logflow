// Entry point. Reads the arguments, assembles the pipeline, runs it.
// No business logic lives here.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "logflow/app/CommandLine.hpp"
#include "logflow/app/PipelineAssembly.hpp"

int main(int argc, char* argv[]) {
    try {
        const logflow::Options options =
            logflow::parseCommandLine(std::vector<std::string>(argv + 1, argv + argc));
        if (options.help) {
            std::cout << logflow::usageText();
            return 0;
        }

        logflow::Pipeline pipeline = logflow::assemblePipeline(options);

        pipeline.run();
    } catch (const logflow::UsageError& e) {
        std::cerr << "logflow: " << e.what() << "\n\n" << logflow::usageText();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "logflow: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
