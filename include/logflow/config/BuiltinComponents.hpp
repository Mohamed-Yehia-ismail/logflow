#pragma once

#include "logflow/config/StageRegistry.hpp"

namespace logflow {

// Registers every component that ships with LogFlow:
//
//   sources: file
//   stages:  parse, filter, project
//   sinks:   file, console
void registerBuiltinComponents(StageRegistry& registry);

// A registry with all built-in components.
StageRegistry builtinRegistry();

}  // namespace logflow
