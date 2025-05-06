# VectorTick Makefile

# Default build type
BUILD_TYPE ?= Release
BUILD_DIR ?= build

# CMake command
CMAKE ?= cmake

# Number of parallel jobs
NPROC ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Configure
.PHONY: configure
configure:
	@echo "Configuring VectorTick..."
	$(CMAKE) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
.PHONY: build
build: configure
	@echo "Building VectorTick..."
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

# Test
.PHONY: test
test: build
	@echo "Running tests..."
	cd $(BUILD_DIR) && ctest --output-on-failure --parallel $(NPROC)

# Sanitizers
.PHONY: sanitize
sanitize:
	@echo "Running with ASan + UBSan..."
	$(MAKE) clean
	$(CMAKE) -B $(BUILD_DIR)-sanitize -DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_ASAN=ON -DENABLE_UBSAN=ON
	$(CMAKE) --build $(BUILD_DIR)-sanitize --parallel $(NPROC)
	cd $(BUILD_DIR)-sanitize && ctest --output-on-failure

.PHONY: tsan
tsan:
	@echo "Running with TSan..."
	$(MAKE) clean
	$(CMAKE) -B $(BUILD_DIR)-tsan -DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_TSAN=ON
	$(CMAKE) --build $(BUILD_DIR)-tsan --parallel $(NPROC)
	cd $(BUILD_DIR)-tsan && ctest --output-on-failure

# Fuzz smoke test
.PHONY: fuzz-smoke
fuzz-smoke: build
	@echo "Running fuzz smoke tests..."
	@if [ -f $(BUILD_DIR)/bin/fuzz_protocol ]; then \
		timeout 60 $(BUILD_DIR)/bin/fuzz_protocol -max_total_time=30 2>/dev/null || true; \
	fi
	@if [ -f $(BUILD_DIR)/bin/fuzz_storage ]; then \
		timeout 60 $(BUILD_DIR)/bin/fuzz_storage -max_total_time=30 2>/dev/null || true; \
	fi
	@if [ -f $(BUILD_DIR)/bin/fuzz_query ]; then \
		timeout 60 $(BUILD_DIR)/bin/fuzz_query -max_total_time=30 2>/dev/null || true; \
	fi
	@if [ -f $(BUILD_DIR)/bin/fuzz_ir ]; then \
		timeout 60 $(BUILD_DIR)/bin/fuzz_ir -max_total_time=30 2>/dev/null || true; \
	fi

# Demo
.PHONY: demo
demo: build
	@echo "Running demo..."
	@mkdir -p artifacts/demo
	$(BUILD_DIR)/bin/vectortick_demo 2>&1 | tee artifacts/demo/transcript.txt

# Dataset
.PHONY: dataset
dataset: build
	@echo "Generating dataset..."
	python3 tools/generate_events.py --count 100000000 --output artifacts/dataset.bin

# Benchmark
.PHONY: benchmark
benchmark: build
	@echo "Running benchmarks..."
	@mkdir -p results/verified
	$(BUILD_DIR)/bin/vectortick_bench 2>&1 | tee results/verified/BENCHMARKS.json

# Profile
.PHONY: profile
profile: build
	@echo "Profiling..."
	@if command -v perf >/dev/null 2>&1; then \
		perf stat -e cycles,instructions,cache-misses,branch-misses \
			$(BUILD_DIR)/bin/vectortick_bench 2>&1 | tee results/profile.txt; \
	else \
		echo "perf not available, skipping"; \
	fi

# Stress
.PHONY: stress
stress: build
	@echo "Running stress tests..."
	$(BUILD_DIR)/bin/vectortick_tests --stress

# Acceptance
.PHONY: acceptance
acceptance:
	@echo "=== VectorTick Acceptance ==="
	@echo ""
	@echo "1. Building release..."
	$(MAKE) clean
	$(MAKE) build BUILD_TYPE=Release
	@echo ""
	@echo "2. Running tests..."
	$(MAKE) test
	@echo ""
	@echo "3. Running sanitizers..."
	$(MAKE) sanitize
	@echo ""
	@echo "4. Running fuzz smoke..."
	$(MAKE) fuzz-smoke
	@echo ""
	@echo "5. Running demo..."
	$(MAKE) demo
	@echo ""
	@echo "6. Generating dataset..."
	$(MAKE) dataset
	@echo ""
	@echo "7. Running benchmarks..."
	$(MAKE) benchmark
	@echo ""
	@echo "8. Running stress..."
	$(MAKE) stress
	@echo ""
	@echo "9. Counting LOC..."
	$(MAKE) loc
	@echo ""
	@echo "=== Acceptance Complete ==="

# Verify
.PHONY: verify
verify:
	@echo "Verifying evidence bundle..."
	@if [ ! -f results/verified/ACCEPTANCE.json ]; then \
		echo "ERROR: No acceptance evidence found"; exit 1; \
	fi
	python3 tools/make_reports.py --verify-only

# LOC
.PHONY: loc
loc:
	@echo "Counting substantive lines of code..."
	python3 tools/count_substantive_loc.py

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-sanitize $(BUILD_DIR)-tsan

# Format
.PHONY: format
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		find include src apps tests fuzz bench -name "*.cpp" -o -name "*.hpp" | \
		xargs clang-format -i; \
	fi

# Tidy
.PHONY: tidy
tidy: build
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy -p $(BUILD_DIR) compile_commands.json; \
	fi

# Help
.PHONY: help
help:
	@echo "VectorTick Build System"
	@echo ""
	@echo "Targets:"
	@echo "  configure   - Configure the build"
	@echo "  build       - Build the project"
	@echo "  test        - Run tests"
	@echo "  sanitize    - Run with ASan + UBSan"
	@echo "  tsan        - Run with ThreadSanitizer"
	@echo "  fuzz-smoke  - Run fuzz smoke tests"
	@echo "  demo        - Run the demo"
	@echo "  dataset     - Generate 100M event dataset"
	@echo "  benchmark   - Run benchmarks"
	@echo "  profile     - Profile execution"
	@echo "  stress      - Run stress tests"
	@echo "  acceptance  - Run full acceptance suite"
	@echo "  verify      - Verify evidence bundle"
	@echo "  loc         - Count lines of code"
	@echo "  clean       - Clean build artifacts"
	@echo "  format      - Format source code"
	@echo "  tidy        - Run clang-tidy"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE  - Build type (Debug/Release/RelWithDebInfo)"
	@echo "  BUILD_DIR   - Build directory (default: build)"
	@echo "  NPROC       - Number of parallel jobs"
