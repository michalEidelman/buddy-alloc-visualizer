#include "buddy_allocator.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
const char* GREEN = "\033[32m";
const char* RED = "\033[31m";
const char* CYAN = "\033[36m";
const char* YELLOW = "\033[33m";
const char* RESET = "\033[0m";
}

int main() {
    auto& allocator = buddy::BuddyAllocator::instance();
    std::unordered_map<int, void*> ids;
    int next_id = 1;

    std::cout << CYAN << "Buddy Allocator Educational Simulator" << RESET << "\n";
    std::cout << "Commands: alloc <bytes>, free <id>, HTML, state, help, quit\n\n";

    std::string line;
    while (true) {
        std::cout << YELLOW << "> " << RESET;
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "alloc") {
            std::size_t bytes = 0;
            in >> bytes;
            void* ptr = allocator.allocate(bytes);
            if (!ptr) {
                std::cout << RED << "allocation failed" << RESET << "\n";
                continue;
            }
            int id = next_id++;
            ids[id] = ptr;
            std::cout << GREEN << "allocated id=" << id << " ptr=" << ptr << RESET << "\n";
            auto events = allocator.consumeLastEvents();
            for (const auto& event : events) {
                std::cout << "  - " << event << "\n";
            }
            // std::cout << allocator.dumpHeapStateJson() << "\n";
        } else if (cmd == "free") {
            int id = 0;
            in >> id;
            auto it = ids.find(id);
            if (it == ids.end()) {
                std::cout << RED << "unknown id" << RESET << "\n";
                continue;
            }
            allocator.deallocate(it->second);
            ids.erase(it);
            std::cout << GREEN << "freed id=" << id << RESET << "\n";
            auto events = allocator.consumeLastEvents();
            for (const auto& event : events) {
                std::cout << "  - " << event << "\n";
            }
            // std::cout << allocator.dumpHeapStateJson() << "\n";
        } else if (cmd == "state") {
            std::cout << allocator.dumpHeapStateJson() << "\n";
        } else if (cmd == "help")
        {
            std::cout << "alloc <bytes>: allocate memory\n"
                      << "free <id>: free a prior allocation\n"
                      << "HTML: creates a visualizer via HTML\n"
                      << "state: dump heap state JSON\n"
                      << "quit: exit\n";
        } else if (cmd == "HTML")
        {
            std::cout << allocator.createHeapStateHTML() << "\n";

        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (!cmd.empty()) {
            std::cout << RED << "unknown command" << RESET << "\n";
        }
    }

    return 0;
}
