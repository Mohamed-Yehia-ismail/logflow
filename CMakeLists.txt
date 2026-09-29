cmake_minimum_required(VERSION 3.16)
project(LogFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(logflow
    src/main.cpp
    src/core/Pipeline.cpp
    src/io/FileLineSource.cpp
)
target_include_directories(logflow PRIVATE include)

if(MSVC)
    target_compile_options(logflow PRIVATE /W4)
else()
    target_compile_options(logflow PRIVATE -Wall -Wextra -Wpedantic)
endif()
