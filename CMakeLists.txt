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

# Everything except main(): shared by the program and the tests.
add_library(logflow_lib
    src/core/Pipeline.cpp
    src/io/ConsoleSink.cpp
    src/io/FileLineSource.cpp
    src/model/LogRecord.cpp
    src/stages/ParserStage.cpp
    src/util/Timestamp.cpp
)
target_include_directories(logflow_lib PUBLIC include)
target_compile_options(logflow_lib PRIVATE ${LOGFLOW_WARNINGS})

add_executable(logflow src/main.cpp)
target_link_libraries(logflow PRIVATE logflow_lib)
target_compile_options(logflow PRIVATE ${LOGFLOW_WARNINGS})

if(LOGFLOW_BUILD_TESTS)
    enable_testing()

    # Use an installed GoogleTest if there is one, otherwise download it.
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(logflow_tests
        tests/ConsoleSinkTest.cpp
        tests/FileLineSourceTest.cpp
        tests/LogRecordTest.cpp
        tests/ParserStageTest.cpp
        tests/PipelineTest.cpp
        tests/TimestampTest.cpp
    )
    target_include_directories(logflow_tests PRIVATE tests)
    target_link_libraries(logflow_tests PRIVATE logflow_lib GTest::gtest_main)
    target_compile_options(logflow_tests PRIVATE ${LOGFLOW_WARNINGS})

    include(GoogleTest)
    gtest_discover_tests(logflow_tests)
endif()
