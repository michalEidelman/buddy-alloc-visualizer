# Makefile for Buddy Allocator Visualizer

CXX = g++
CXXFLAGS = -Wno-unused-result -std=c++17 -Iinclude
BUILD_DIR = build
TEST_DIR = tests

TEST1_BIN = $(BUILD_DIR)/test1
TEST2_BIN = $(BUILD_DIR)/test2
TEST3_BIN = $(BUILD_DIR)/test3
TEST4_BIN = $(BUILD_DIR)/test4
SIM_BIN = $(BUILD_DIR)/simulator
BENCH_BIN = $(BUILD_DIR)/benchmark

BUDDY_SRC = src/buddy_allocator.cpp

TEST1_SRC = $(TEST_DIR)/buddy_api_test_1.cpp
TEST2_SRC = $(TEST_DIR)/buddy_api_test_2.cpp
TEST3_SRC = $(TEST_DIR)/buddy_api_test_3.cpp
TEST4_SRC = $(TEST_DIR)/buddy_api_test_4.cpp

HEADER = os_malloc.h

.PHONY: all-tests clean check-os help simulator benchmark run

help:
	@echo "========================================="
	@echo "Buddy Allocator Visualizer"
	@echo "========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  make run  		 - Build and run interactive simulator"
	@echo "  make simulator  - Build interactive simulator"
	@echo "  make benchmark  - Build benchmark binary"
	@echo "  make all-tests  - Build and run all tests"
	@echo "  make clean      - Remove compiled binaries and zip files"
	@echo "  make check-os   - Check OS compatibility"
	@echo ""

check-os:
	@if [ -f /etc/os-release ]; then \
		. /etc/os-release; \
		if [ "$$VERSION_ID" = "18.04" ]; then \
			echo "Ubuntu 18.04"; \
		else \
			echo "Not Ubuntu 18.04"; \
		fi \
	else \
		echo "Cannot detect OS"; \
	fi

all-tests:
	test1 test2 test3 test4

test1: check-os
	@if [ ! -f $(BUDDY_SRC) ]; then \
		echo "ERROR: $(BUDDY_SRC) not found!"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(BUDDY_SRC)..."
	$(CXX) $(BUDDY_SRC) $(TEST1_SRC) $(CXXFLAGS) -o $(TEST1_BIN)
	@echo "Running test_1..."
	@./$(TEST1_BIN)

test2: check-os
	@if [ ! -f $(BUDDY_SRC) ]; then \
		echo "ERROR: $(BUDDY_SRC) not found!"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(BUDDY_SRC)..."
	$(CXX) $(BUDDY_SRC) $(TEST2_SRC) $(CXXFLAGS) -o $(TEST2_BIN)
	@echo "Running test_2..."
	@./$(TEST2_BIN)

test3: check-os
	@if [ ! -f $(BUDDY_SRC) ]; then \
		echo "ERROR: $(BUDDY_SRC) not found!"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(BUDDY_SRC)"
	$(CXX) $(BUDDY_SRC) $(TEST3_SRC) $(CXXFLAGS) -o $(TEST3_BIN)
	@echo "Running test_3..."
	@./$(TEST3_BIN)

test4: check-os
	@if [ ! -f $(BUDDY_SRC) ]; then \
		echo "ERROR: $(BUDDY_SRC) not found!"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(BUDDY_SRC)..."
	$(CXX) $(BUDDY_SRC) $(TEST4_SRC) $(CXXFLAGS) -o $(TEST4_BIN)
	@echo "Running test_4 tests..."
	@./$(TEST4_BIN)

run: simulator
	@./build/simulator

simulator:
	@mkdir -p $(BUILD_DIR)
	$(CXX) tools/simulator.cpp $(BUDDY_SRC) $(CXXFLAGS) -o $(SIM_BIN)

benchmark:
	@mkdir -p $(BUILD_DIR)
	$(CXX) benchmarks/benchmark.cpp $(BUDDY_SRC) $(CXXFLAGS) -O2 -o $(BENCH_BIN)

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)
	rm -f *.zip
	rm -f *json
	@echo "Done."
