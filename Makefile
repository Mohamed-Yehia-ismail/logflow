# Convenience targets. The build itself is done by CMake (see CMakeLists.txt).
# Usage: make, make run, make test, make coverage, make experiment, make clean
BUILD := build/cmake

.PHONY: all run test coverage experiment clean

all:
	@cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release >/dev/null
	@cmake --build $(BUILD) --target logflow -j

run: all
	./$(BUILD)/logflow --config configs/error-report.yaml && cat out/errors.txt

test:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j
	ctest --test-dir $(BUILD) --output-on-failure

coverage:
	./scripts/coverage.sh

experiment:
	cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DLOGFLOW_BUILD_TESTS=OFF
	cmake --build build/release --target logflow_ordering_experiment -j
	./build/release/logflow_ordering_experiment data/access-small.log

clean:
	rm -rf build out
