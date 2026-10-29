#include "buddy_allocator.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>

namespace buddy {

    BuddyAllocator& BuddyAllocator::instance() {
        static BuddyAllocator allocator;
        return allocator;
    }

    BuddyAllocator::BuddyAllocator()
        : mmap_list_(nullptr),
          base_(nullptr),
          initialized_(false),
          total_buddy_blocks_(0),
          total_mmap_blocks_(0),
          total_mmap_payload_bytes_(0) {
        for (int i = 0; i <= MAX_ORDER; ++i) {
            free_lists_[i] = nullptr;
        }
    }

    int BuddyAllocator::calcRequiredOrder(std::size_t size) const {
        int order = MIN_ORDER;
        while ((MIN_BLOCK << order) < size) {
            ++order;
        }
        return (order <= MAX_ORDER) ? order : -1;
    }

    bool BuddyAllocator::initStructure() {
        if (initialized_) {
            return true;
        }

        void* curr_brk = sbrk(0);
        std::size_t addr = reinterpret_cast<std::size_t>(curr_brk);
        std::size_t remainder = addr % INIT_SIZE;
        std::size_t gap = (remainder == 0) ? 0 : (INIT_SIZE - remainder);

        if (gap > 0 && sbrk(gap) == reinterpret_cast<void*>(-1)) {
            return false;
        }

        base_ = sbrk(INIT_SIZE);
        if (base_ == reinterpret_cast<void*>(-1)) {
            base_ = nullptr;
            return false;
        }

        for (int i = 0; i < 32; ++i) {
            auto* block = reinterpret_cast<MallocMetadata*>(reinterpret_cast<char*>(base_) + i * MAX_BLOCK);
            block->size = MAX_BLOCK;
            block->requested_size = 0;
            block->next = nullptr;
            block->prev = nullptr;
            block->is_free = true;
            insertFree(block);
        }

        total_buddy_blocks_ += 32;
        initialized_ = true;
        return true;
    }

    void BuddyAllocator::insertFree(MallocMetadata* block) {
        block->is_free = true;
        block->requested_size = 0;
        int order = calcRequiredOrder(block->size);
        MallocMetadata* curr = free_lists_[order];

        if (!curr) {
            block->prev = nullptr;
            block->next = nullptr;
            free_lists_[order] = block;
            return;
        }

        if (curr > block) {
            block->prev = nullptr;
            block->next = curr;
            curr->prev = block;
            free_lists_[order] = block;
            return;
        }

        while (curr->next && curr->next < block) {
            curr = curr->next;
        }

        block->prev = curr;
        block->next = curr->next;
        if (curr->next) {
            curr->next->prev = block;
        }
        curr->next = block;
    }

    void BuddyAllocator::removeFree(MallocMetadata* block) {
        block->is_free = false;
        int order = calcRequiredOrder(block->size);
        MallocMetadata* curr = free_lists_[order];

        if (!curr) {
            return;
        }

        if (curr == block) {
            free_lists_[order] = curr->next;
            if (free_lists_[order]) {
                free_lists_[order]->prev = nullptr;
            }
            curr->next = nullptr;
            curr->prev = nullptr;
            return;
        }

        while (curr && curr != block) {
            curr = curr->next;
        }

        if (!curr) {
            return;
        }

        if (curr->prev) {
            curr->prev->next = curr->next;
        }
        if (curr->next) {
            curr->next->prev = curr->prev;
        }
        curr->next = nullptr;
        curr->prev = nullptr;
    }

    void BuddyAllocator::splitDown(MallocMetadata* block, int req_order) {
        while (block->size > (MIN_BLOCK << req_order)) {
            block->size /= 2;

            auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<char*>(block) + block->size);
            buddy->size = block->size;
            buddy->requested_size = 0;
            buddy->next = nullptr;
            buddy->prev = nullptr;
            buddy->is_free = true;
            insertFree(buddy);
            total_buddy_blocks_++;

            std::ostringstream os;
            os << "split order=" << calcRequiredOrder(block->size * 2)
               << " into order=" << calcRequiredOrder(block->size)
               << " at " << ptrToHex(block) << " and " << ptrToHex(buddy);
            last_events_.push_back(os.str());
        }
    }

    void BuddyAllocator::mergeBuddies(MallocMetadata* block) {
        auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<std::size_t>(block) ^ block->size);
        while (buddy->is_free && buddy->size == block->size && buddy->size < MAX_BLOCK) {
            removeFree(buddy);
            std::size_t old_size = block->size;
            if (reinterpret_cast<std::size_t>(block) > reinterpret_cast<std::size_t>(buddy)) {
                block = buddy;
            }
            block->size *= 2;
            block->requested_size = 0;
            total_buddy_blocks_--;

            std::ostringstream os;
            os << "coalesce order=" << calcRequiredOrder(old_size)
               << " -> order=" << calcRequiredOrder(block->size)
               << " using XOR buddy " << ptrToHex(buddy);
            last_events_.push_back(os.str());

            buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<std::size_t>(block) ^ block->size);
        }
        insertFree(block);
    }

    void* BuddyAllocator::bigAlloc(std::size_t size) {
        std::size_t total_size = size + sizeof(MallocMetadata);
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    #ifdef MAP_HUGETLB
        void* mapped = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags | MAP_HUGETLB, -1, 0);
        if (mapped == MAP_FAILED) {
            mapped = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);
        }
    #else
        void* mapped = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);
    #endif
        if (mapped == MAP_FAILED) {
            return nullptr;
        }

        auto* block = reinterpret_cast<MallocMetadata*>(mapped);
        block->size = total_size;
        block->requested_size = size;
        block->is_free = false;
        block->prev = nullptr;
        block->next = mmap_list_;

        if (mmap_list_) {
            mmap_list_->prev = block;
        }
        mmap_list_ = block;

        total_mmap_blocks_++;
        total_mmap_payload_bytes_ += size;

        std::ostringstream os;
        os << "mmap alloc size=" << size << " at " << ptrToHex(block);
        last_events_.push_back(os.str());

        return reinterpret_cast<char*>(block) + sizeof(MallocMetadata);
    }

    void BuddyAllocator::bigFree(MallocMetadata* block) {
        if (mmap_list_ == block) {
            mmap_list_ = block->next;
            if (mmap_list_) {
                mmap_list_->prev = nullptr;
            }
        } else {
            if (block->prev) {
                block->prev->next = block->next;
            }
            if (block->next) {
                block->next->prev = block->prev;
            }
        }

        total_mmap_blocks_--;
        total_mmap_payload_bytes_ -= block->requested_size;

        std::ostringstream os;
        os << "munmap free size=" << block->requested_size << " at " << ptrToHex(block);
        last_events_.push_back(os.str());

        munmap(block, block->size);
    }

    bool BuddyAllocator::canMergeForResize(MallocMetadata* block, MallocMetadata* buddy, std::size_t size) const {
        MallocMetadata* head = block;
        std::size_t total_size = block->size;

        while (buddy->is_free && buddy->size == total_size && buddy->size < MAX_BLOCK) {
            if (reinterpret_cast<std::size_t>(head) > reinterpret_cast<std::size_t>(buddy)) {
                head = buddy;
            }
            total_size *= 2;
            if (total_size >= size + sizeof(MallocMetadata)) {
                return true;
            }
            buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<std::size_t>(head) ^ total_size);
        }
        return false;
    }

    void* BuddyAllocator::tryMergeForResize(void* old_ptr, std::size_t size) {
        auto* block = reinterpret_cast<MallocMetadata*>(reinterpret_cast<char*>(old_ptr) - sizeof(MallocMetadata));
        std::size_t original_size = block->size;

        auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<std::size_t>(block) ^ block->size);
        if (!canMergeForResize(block, buddy, size)) {
            return nullptr;
        }

        while (buddy->is_free && buddy->size == block->size && buddy->size < MAX_BLOCK) {
            removeFree(buddy);
            std::size_t old_size = block->size;
            if (reinterpret_cast<std::size_t>(block) > reinterpret_cast<std::size_t>(buddy)) {
                block = buddy;
            }
            block->size *= 2;
            total_buddy_blocks_--;

            std::ostringstream os;
            os << "realloc-coalesce order=" << calcRequiredOrder(old_size)
               << " -> order=" << calcRequiredOrder(block->size)
               << " using XOR buddy " << ptrToHex(buddy);
            last_events_.push_back(os.str());

            if (block->size >= size + sizeof(MallocMetadata)) {
                break;
            }
            buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<std::size_t>(block) ^ block->size);
        }

        std::memmove(reinterpret_cast<char*>(block) + sizeof(MallocMetadata), old_ptr, original_size - sizeof(MallocMetadata));
        block->is_free = false;
        block->requested_size = size;
        return reinterpret_cast<char*>(block) + sizeof(MallocMetadata);
    }

    void* BuddyAllocator::allocateLocked(std::size_t size) {
        if (size == 0 || size > MAX_REQUEST) {
            return nullptr;
        }
        if (!initStructure()) {
            return nullptr;
        }

        int req_order = calcRequiredOrder(size + sizeof(MallocMetadata));
        if (req_order == -1) {
            return bigAlloc(size);
        }

        int target_order = req_order;
        while (req_order <= MAX_ORDER && !free_lists_[req_order]) {
            ++req_order;
        }

        if (req_order > MAX_ORDER) {
            return nullptr;
        }

        MallocMetadata* block = free_lists_[req_order];
        removeFree(block);
        splitDown(block, target_order);
        block->is_free = false;
        block->requested_size = size;

        std::ostringstream os;
        os << "alloc size=" << size << " order=" << target_order << " at " << ptrToHex(block);
        last_events_.push_back(os.str());

        return reinterpret_cast<char*>(block) + sizeof(MallocMetadata);
    }

    void* BuddyAllocator::allocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocateLocked(size);
    }

    void* BuddyAllocator::callocate(std::size_t num, std::size_t size) {
        if (num == 0 || size == 0) {
            return nullptr;
        }
        if (num > MAX_REQUEST / size) {
            return nullptr;
        }

        void* ptr = allocate(num * size);
        if (ptr) {
            std::memset(ptr, 0, num * size);
        }
        return ptr;
    }

    void BuddyAllocator::deallocateLocked(void* ptr) {
        if (!ptr) {
            return;
        }
        auto* block = reinterpret_cast<MallocMetadata*>(reinterpret_cast<char*>(ptr) - sizeof(MallocMetadata));

        if (block->is_free) {
            return;
        }

        if (block->size > MAX_BLOCK) {
            bigFree(block);
            return;
        }

        block->requested_size = 0;
        mergeBuddies(block);
    }

    void BuddyAllocator::deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        deallocateLocked(ptr);
    }

    void* BuddyAllocator::reallocate(void* ptr, std::size_t size) {
        if (size == 0 || size > MAX_REQUEST) {
            return nullptr;
        }
        if (!ptr) {
            return allocate(size);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto* block = reinterpret_cast<MallocMetadata*>(reinterpret_cast<char*>(ptr) - sizeof(MallocMetadata));
        if (block->size > MAX_BLOCK) {
            if (size + sizeof(MallocMetadata) == block->size) {
                block->requested_size = size;
                return ptr;
            }

            void* new_ptr = allocateLocked(size);
            if (!new_ptr) {
                return nullptr;
            }

            std::size_t copy_size = block->requested_size;
            std::memmove(new_ptr, ptr, copy_size);
            bigFree(block);
            return new_ptr;
        }

        if (size + sizeof(MallocMetadata) <= block->size) {
            block->requested_size = size;
            return ptr;
        }

        void* merged = tryMergeForResize(ptr, size);
        if (merged) {
            return merged;
        }

        void* new_ptr = allocateLocked(size);
        if (!new_ptr) {
            return nullptr;
        }

        std::size_t copy_size = block->requested_size;
        std::memmove(new_ptr, ptr, copy_size);
        block->requested_size = 0;
        mergeBuddies(block);
        return new_ptr;
    }

    std::size_t BuddyAllocator::numFreeBlocks() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (int i = MIN_ORDER; i <= MAX_ORDER; ++i) {
            MallocMetadata* curr = free_lists_[i];
            while (curr) {
                ++count;
                curr = curr->next;
            }
        }
        return count;
    }

    std::size_t BuddyAllocator::numFreeBytes() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (int i = MIN_ORDER; i <= MAX_ORDER; ++i) {
            MallocMetadata* curr = free_lists_[i];
            while (curr) {
                count += (MIN_BLOCK << i) - sizeof(MallocMetadata);
                curr = curr->next;
            }
        }
        return count;
    }

    std::size_t BuddyAllocator::numAllocatedBlocks() {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_buddy_blocks_ + total_mmap_blocks_;
    }

    std::size_t BuddyAllocator::numAllocatedBytes() {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_ ? (INIT_SIZE - total_buddy_blocks_ * sizeof(MallocMetadata)) + total_mmap_payload_bytes_ : 0;
    }

    std::size_t BuddyAllocator::numMetadataBytes() {
        std::lock_guard<std::mutex> lock(mutex_);
        return (total_buddy_blocks_ + total_mmap_blocks_) * sizeof(MallocMetadata);
    }

    std::size_t BuddyAllocator::metadataSize() const {
        return sizeof(MallocMetadata);
    }

    std::string BuddyAllocator::ptrToHex(const void* ptr) {
        std::ostringstream os;
        os << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(ptr);
        return os.str();
    }

    void BuddyAllocator::appendJsonStringEscaped(std::string& out, const std::string& value) {
        out.push_back('"');
        for (char c : value) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        out.push_back('"');
    }

    std::vector<std::string> BuddyAllocator::consumeLastEvents() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> events;
        events.swap(last_events_);
        return events;
    }

    std::string BuddyAllocator::dumpHeapStateJson() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            initStructure();
        }

        std::string json;
        json.reserve(8192);

        std::size_t allocated_blocks = 0;
        std::size_t free_blocks = 0;
        std::size_t allocated_payload = 0;
        std::size_t requested_payload = 0;

        json += "{\"orders\":[";

        for (int order = MIN_ORDER; order <= MAX_ORDER; ++order) {
            if (order > MIN_ORDER) {
                json += ",";
            }

            std::size_t block_size = (MIN_BLOCK << order);
            std::size_t order_allocated = 0;
            std::size_t order_free = 0;
            std::size_t order_internal_frag = 0;
            bool first_block = true;

            json += "\n \t {\"order\":" + std::to_string(order)
                 + ",\"blockSize\":" + std::to_string(block_size)
                 + ",\"blocks\":[ \n";

            char* cursor = reinterpret_cast<char*>(base_);
            char* limit = reinterpret_cast<char*>(base_) + INIT_SIZE;
            while (cursor < limit) {
                auto* block = reinterpret_cast<MallocMetadata*>(cursor);
                if (block->size == block_size) {
                    if (!first_block) {
                        json += ", \n";
                    }
                    first_block = false;

                    bool is_allocated = !block->is_free;
                    if (is_allocated) {
                        ++order_allocated;
                        ++allocated_blocks;
                        allocated_payload += (block->size - sizeof(MallocMetadata));
                        requested_payload += block->requested_size;
                        order_internal_frag += (block->size - sizeof(MallocMetadata) - block->requested_size);
                    } else {
                        ++order_free;
                        ++free_blocks;
                    }

                    json += "\t \t {\"start\":";
                    appendJsonStringEscaped(json, ptrToHex(block));
                    json += ",\"size\":" + std::to_string(block->size)
                         + ",\"status\":";
                    appendJsonStringEscaped(json, is_allocated ? "allocated" : "free");
                    json += ",\"requested\":" + std::to_string(block->requested_size)
                         + ",\"internalFragmentation\":"
                         + std::to_string(is_allocated ? (block->size - sizeof(MallocMetadata) - block->requested_size) : 0)
                         + "}";
                }
                cursor += block->size;
            }

            json += "], \n \t \t \"summary\":{\"total\":" + std::to_string(order_allocated + order_free)
                 + ",\"allocated\":" + std::to_string(order_allocated)
                 + ",\"free\":" + std::to_string(order_free)
                 + ",\"internalFragmentation\":" + std::to_string(order_internal_frag)
                 + "}}";
        }

        std::size_t mmap_allocated = 0;
        for (MallocMetadata* curr = mmap_list_; curr; curr = curr->next) {
            ++mmap_allocated;
            ++allocated_blocks;
            allocated_payload += curr->requested_size;
            requested_payload += curr->requested_size;
        }

        std::size_t internal_fragmentation = allocated_payload > requested_payload ? (allocated_payload - requested_payload) : 0;

        json += "], \n \t \"metrics\":{";
        json += "\"freeBlocks\":" + std::to_string(free_blocks) + ",";
        json += "\"allocatedBlocks\":" + std::to_string(allocated_blocks) + ",";
        json += "\"mmapAllocatedBlocks\":" + std::to_string(mmap_allocated) + ",";
        json += "\"requestedBytes\":" + std::to_string(requested_payload) + ",";
        json += "\"allocatedPayloadBytes\":" + std::to_string(allocated_payload) + ",";
        json += "\"internalFragmentationBytes\":" + std::to_string(internal_fragmentation) + ",";
        json += "\"internalFragmentationRatio\":";
        if (allocated_payload == 0) {
            json += "0.0";
        } else {
            std::ostringstream ratio;
            ratio.setf(std::ios::fixed);
            ratio.precision(6);
            ratio << static_cast<double>(internal_fragmentation) / static_cast<double>(allocated_payload);
            json += ratio.str();
        }
        json += "}}";

        std::ofstream file("build/heap_state.json");
        if (file.is_open()) {
            file << json;
            file.close();
        }

        return json;
    }


    std::string BuddyAllocator::createHeapStateHTML(){
        dumpHeapStateJson();

        int result = std::system("python3 tools/visualizer.py build/heap_state.json > build/heap_visual.html");
        if (result != 0)
        {
            std::cerr << "Error: Failed to generate HTML visualizer. "
                         "Please ensure that Python 3 and the required libraries are installed." << std::endl;
            return "";
        }
        return "HTML visualizer generated: heap_visual.html";

    }

} // namespace buddy
