# Builds build/logflow. Usage: make, make run, make test, make coverage, make experiment, make clean
# The program builds with make alone; the tests use CMake and GoogleTest.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:src/%.cpp=build/obj/%.o)
DEPS := $(OBJS:.o=.d)
BIN  := build/logflow

.PHONY: all run test coverage experiment clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(BIN)
	./$(BIN) data/access-small.log

test:
	cmake -S . -B build/cmake
	cmake --build build/cmake --target logflow_tests -j
	ctest --test-dir build/cmake --output-on-failure

coverage:
	./scripts/coverage.sh

experiment:
	cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DLOGFLOW_BUILD_TESTS=OFF
	cmake --build build/release --target logflow_ordering_experiment -j
	./build/release/logflow_ordering_experiment data/access-small.log

clean:
	rm -rf build

-include $(DEPS)
