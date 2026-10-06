#!/usr/bin/env bash
# Builds the tests with clang coverage instrumentation, runs them, and prints a
# line-coverage report for the LogFlow sources (tests and GoogleTest excluded).
# Requires clang with llvm-profdata and llvm-cov (on macOS they come with Xcode).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/coverage"

if command -v xcrun >/dev/null 2>&1; then
    PROFDATA=(xcrun llvm-profdata)
    COV=(xcrun llvm-cov)
else
    PROFDATA=(llvm-profdata)
    COV=(llvm-cov)
fi

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
      -DLOGFLOW_COVERAGE=ON >/dev/null
cmake --build "$BUILD" --target logflow_tests -j >/dev/null

rm -f "$BUILD"/*.profraw
LLVM_PROFILE_FILE="$BUILD/tests.profraw" "$BUILD/logflow_tests" --gtest_brief=1
"${PROFDATA[@]}" merge -sparse "$BUILD/tests.profraw" -o "$BUILD/tests.profdata"

"${COV[@]}" report "$BUILD/logflow_tests" -instr-profile="$BUILD/tests.profdata" \
    -ignore-filename-regex='(/tests/|/_deps/|googletest|gtest|/opt/homebrew/|/usr/)' \
    -show-region-summary=false -show-branch-summary=false -show-instantiation-summary=false
