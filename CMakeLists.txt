cmake_minimum_required(VERSION 3.16)
project(LogFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOGFLOW_BUILD_TESTS "Build the unit tests" ON)
option(LOGFLOW_COVERAGE "Instrument the build for line coverage (clang)" OFF)

if(MSVC)
    set(LOGFLOW_WARNINGS /W4)
else()
    set(LOGFLOW_WARNINGS -Wall -Wextra -Wpedantic)
endif()

if(LOGFLOW_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

include(FetchContent)

# YAML parser for pipeline configurations. Use an installed yaml-cpp if there is one,
# otherwise download it.
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_Declare(yaml-cpp URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz)
    FetchContent_MakeAvailable(yaml-cpp)
endif()
if(TARGET yaml-cpp::yaml-cpp)
    set(LOGFLOW_YAML yaml-cpp::yaml-cpp)
else()
    set(LOGFLOW_YAML yaml-cpp)
endif()

# Everything except main(): shared by the program and the tests.
add_library(logflow_lib
    src/app/CommandLine.cpp
    src/app/PipelineAssembly.cpp
    src/config/BuiltinComponents.cpp
    src/config/Params.cpp
    src/config/PipelineLoader.cpp
    src/core/ErrorPolicy.cpp
    src/core/Pipeline.cpp
    src/core/RunSummary.cpp
    src/io/FileLineSource.cpp
    src/io/LineFormat.cpp
    src/model/LogRecord.cpp
    src/model/LogRecordFields.cpp
    src/stages/ParserStage.cpp
    src/stages/Predicates.cpp
    src/stages/ProjectionStage.cpp
    src/stages/ValidationStage.cpp
    src/util/Timestamp.cpp
)
target_include_directories(logflow_lib PUBLIC include)
target_link_libraries(logflow_lib PRIVATE ${LOGFLOW_YAML})
target_compile_options(logflow_lib PRIVATE ${LOGFLOW_WARNINGS})

add_executable(logflow src/main.cpp)
target_link_libraries(logflow PRIVATE logflow_lib)
target_compile_options(logflow PRIVATE ${LOGFLOW_WARNINGS})

# Ordering experiment (Increment 3). Build with -DCMAKE_BUILD_TYPE=Release for timings.
add_executable(logflow_ordering_experiment experiments/OrderingExperiment.cpp)
target_link_libraries(logflow_ordering_experiment PRIVATE logflow_lib)
target_compile_options(logflow_ordering_experiment PRIVATE ${LOGFLOW_WARNINGS})

if(LOGFLOW_BUILD_TESTS)
    enable_testing()

    # Use an installed GoogleTest if there is one, otherwise download it.
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(logflow_tests
        tests/ChaosTest.cpp
        tests/CommandLineTest.cpp
        tests/CompareTest.cpp
        tests/ConfigFilesTest.cpp
        tests/ConsoleSinkTest.cpp
        tests/ErrorPolicyTest.cpp
        tests/FileLineSourceTest.cpp
        tests/FileSinkTest.cpp
        tests/IntegrationTest.cpp
        tests/LineFormatTest.cpp
        tests/LogRecordTest.cpp
        tests/ParserStageTest.cpp
        tests/PipelineBuilderTest.cpp
        tests/PipelineLoaderTest.cpp
        tests/PipelineTest.cpp
        tests/PredicateFilterStageTest.cpp
        tests/ProjectionStageTest.cpp
        tests/RunSummaryTest.cpp
        tests/StageRegistryTest.cpp
        tests/TimestampTest.cpp
        tests/ValidationStageTest.cpp
    )
    target_include_directories(logflow_tests PRIVATE tests)
    target_compile_definitions(logflow_tests PRIVATE
        LOGFLOW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        LOGFLOW_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(logflow_tests PRIVATE logflow_lib GTest::gtest_main)
    target_compile_options(logflow_tests PRIVATE ${LOGFLOW_WARNINGS})

    include(GoogleTest)
    gtest_discover_tests(logflow_tests)
endif()
