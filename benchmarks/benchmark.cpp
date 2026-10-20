#include "BuddyAllocator.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

struct Scenario {
    const char* name;
    std::size_t alloc_size;
    std::size_t operations;
};

void runScenario(const Scenario& scenario) {
    auto& allocator = buddy::BuddyAllocator::instance();
    std::vector<void*> ptrs;
    ptrs.reserve(scenario.operations);

    auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < scenario.operations; ++i) {
        void* p = allocator.allocate(scenario.alloc_size);
        if (p) {
            ptrs.push_back(p);
        }
    }
    auto mid = std::chrono::steady_clock::now();

    for (void* p : ptrs) {
        allocator.deallocate(p);
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double> alloc_sec = mid - start;
    std::chrono::duration<double> free_sec = end - mid;

    std::size_t allocated_bytes = allocator.numAllocatedBytes();
    std::size_t metadata_bytes = allocator.numMetadataBytes();
    double efficiency = allocated_bytes == 0
        ? 1.0
        : static_cast<double>(allocated_bytes) / static_cast<double>(allocated_bytes + metadata_bytes);

    std::cout << "Scenario: " << scenario.name << "\n"
              << "  operations: " << ptrs.size() << "\n"
              << "  alloc throughput ops/s: " << std::fixed << std::setprecision(2)
              << (ptrs.empty() ? 0.0 : ptrs.size() / alloc_sec.count()) << "\n"
              << "  free throughput ops/s: "
              << (ptrs.empty() ? 0.0 : ptrs.size() / free_sec.count()) << "\n"
              << "  memory efficiency: " << std::setprecision(4) << efficiency << "\n\n";
}

} // namespace

int main() {
    std::cout << "Buddy allocator benchmark\n\n";

    const Scenario scenarios[] = {
        {"Tiny allocations", 64, 50000},
        {"Small allocations", 256, 40000},
        {"Medium allocations", 4096, 20000},
        {"Large allocations via mmap", 256 * 1024, 2000},
    };

    for (const auto& scenario : scenarios) {
        runScenario(scenario);
    }

    return 0;
}
