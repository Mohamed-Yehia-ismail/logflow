# Builds build/logflow. Usage: make, make run, make test, make coverage, make clean
# The program builds with make alone; the tests use CMake and GoogleTest.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:src/%.cpp=build/obj/%.o)
DEPS := $(OBJS:.o=.d)
BIN  := build/logflow

.PHONY: all run test coverage clean

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

clean:
	rm -rf build

-include $(DEPS)
