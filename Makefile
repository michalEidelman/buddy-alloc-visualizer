# Makefile for Buddy Allocator Visualizer

CXX = g++
CXXFLAGS = -Wno-unused-result -std=c++17 -Iinclude

TEST1_BIN = test1
TEST2_BIN = test2
TEST3_BIN = test3
TEST4_BIN = test4
SIM_BIN = simulator
BENCH_BIN = benchmark

MALLOC1_SRC = malloc_1.cpp
MALLOC2_SRC = malloc_2.cpp
MALLOC3_SRC = malloc_3.cpp
MALLOC4_SRC = malloc_4.cpp
BUDDY_SRC = src/BuddyAllocator.cpp
SUBMITTERS = submitters.txt

TEST1_SRC = test_malloc_1.cpp
TEST2_SRC = test_malloc_2.cpp
TEST3_SRC = test_malloc_3.cpp
TEST4_SRC = test_malloc_4.cpp

HEADER = os_malloc.h

.PHONY: all clean check-os submit help test1 test2 test3 test4 simulator benchmark

all: test1 test2 test3

help:
	@echo "========================================="
	@echo "Buddy Allocator Visualizer"
	@echo "========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  make test1      - Test malloc_1 implementation"
	@echo "  make test2      - Test malloc_2 implementation"
	@echo "  make test3      - Test malloc_3 implementation"
	@echo "  make test4      - Test malloc_4 implementation (optional)"
	@echo "  make simulator  - Build interactive simulator"
	@echo "  make benchmark  - Build benchmark binary"
	@echo "  make all        - Run tests 1, 2, and 3"
	@echo "  make submit     - Create submission zip file"
	@echo "  make clean      - Remove compiled binaries and zip files"
	@echo "  make check-os   - Check OS compatibility"
	@echo ""

check-os:
	@if [ -f /etc/os-release ]; then \
		. /etc/os-release; \
		if [ "$$VERSION_ID" = "18.04" ]; then \
			echo "✓ Ubuntu 18.04"; \
		else \
			echo "⚠ Not Ubuntu 18.04 (grading uses Ubuntu 18.04)"; \
		fi \
	else \
		echo "⚠ Cannot detect OS (grading uses Ubuntu 18.04)"; \
	fi

test1: check-os
	@if [ ! -f $(MALLOC1_SRC) ]; then \
		echo "ERROR: $(MALLOC1_SRC) not found!"; \
		exit 1; \
	fi
	@echo "Compiling $(MALLOC1_SRC)..."
	$(CXX) $(MALLOC1_SRC) $(TEST1_SRC) $(CXXFLAGS) -o $(TEST1_BIN)
	@echo "Running malloc_1 tests..."
	@./$(TEST1_BIN)

test2: check-os
	@if [ ! -f $(MALLOC2_SRC) ]; then \
		echo "ERROR: $(MALLOC2_SRC) not found!"; \
		exit 1; \
	fi
	@echo "Compiling $(MALLOC2_SRC)..."
	$(CXX) $(MALLOC2_SRC) $(TEST2_SRC) $(CXXFLAGS) -o $(TEST2_BIN)
	@echo "Running malloc_2 tests..."
	@./$(TEST2_BIN)

test3: check-os
	@if [ ! -f $(MALLOC3_SRC) ]; then \
		echo "ERROR: $(MALLOC3_SRC) not found!"; \
		exit 1; \
	fi
	@echo "Compiling malloc_3 + BuddyAllocator library..."
	$(CXX) $(MALLOC3_SRC) $(BUDDY_SRC) $(TEST3_SRC) $(CXXFLAGS) -o $(TEST3_BIN)
	@echo "Running malloc_3 tests..."
	@./$(TEST3_BIN)

test4: check-os
	@if [ ! -f $(MALLOC4_SRC) ]; then \
		echo "NOTE: $(MALLOC4_SRC) not found (this is optional)."; \
		exit 0; \
	fi
	@echo "Compiling $(MALLOC4_SRC)..."
	$(CXX) $(MALLOC4_SRC) $(TEST4_SRC) $(CXXFLAGS) -o $(TEST4_BIN)
	@echo "Running malloc_4 tests..."
	@./$(TEST4_BIN)

simulator:
	$(CXX) tools/simulator.cpp $(BUDDY_SRC) $(CXXFLAGS) -o $(SIM_BIN)

benchmark:
	$(CXX) benchmark.cpp $(BUDDY_SRC) $(CXXFLAGS) -O2 -o $(BENCH_BIN)

submit:
	@echo "========================================="
	@echo "Creating Submission Package"
	@echo "========================================="
	@if [ ! -f $(SUBMITTERS) ]; then \
		echo "ERROR: $(SUBMITTERS) not found!"; \
		exit 1; \
	fi
	@ID1=$$(sed -n '1p' $(SUBMITTERS) | awk '{print $$NF}' | tr -d ' \t\r\n'); \
	ID2=$$(sed -n '2p' $(SUBMITTERS) | awk '{print $$NF}' | tr -d ' \t\r\n'); \
	if [ -z "$$ID1" ] || [ -z "$$ID2" ]; then \
		echo "ERROR: $(SUBMITTERS) must contain exactly 2 student IDs!"; \
		exit 1; \
	fi; \
	ZIPNAME="$$ID1-$$ID2.zip"; \
	FILES_TO_ZIP="$(SUBMITTERS) $(MALLOC1_SRC) $(MALLOC2_SRC) $(MALLOC3_SRC) $(MALLOC4_SRC) os_malloc.h include/BuddyAllocator.hpp src/BuddyAllocator.cpp README.md"; \
	zip -q $$ZIPNAME $$FILES_TO_ZIP; \
	echo "✓ Submission created: $$ZIPNAME"

clean:
	@echo "Cleaning up..."
	rm -f $(TEST1_BIN) $(TEST2_BIN) $(TEST3_BIN) $(TEST4_BIN) $(SIM_BIN) $(BENCH_BIN)
	rm -f *.zip
	@echo "Done."
