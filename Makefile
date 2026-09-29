# Builds build/logflow. Usage: make, make run, make clean
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:src/%.cpp=build/obj/%.o)
DEPS := $(OBJS:.o=.d)
BIN  := build/logflow

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(BIN)
	./$(BIN) data/access-small.log

clean:
	rm -rf build

-include $(DEPS)
