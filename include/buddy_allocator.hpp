#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace buddy {

class BuddyAllocator {
public:
    static BuddyAllocator& instance();

    void* allocate(std::size_t size);
    void* callocate(std::size_t num, std::size_t size);
    void deallocate(void* ptr);
    void* reallocate(void* ptr, std::size_t size);

    std::size_t numFreeBlocks();
    std::size_t numFreeBytes();
    std::size_t numAllocatedBlocks();
    std::size_t numAllocatedBytes();
    std::size_t numMetadataBytes();
    std::size_t metadataSize() const;

    std::string dumpHeapStateJson();
    std::string createHeapStateHTML();
    std::vector<std::string> consumeLastEvents();


private:
    BuddyAllocator();

    struct MallocMetadata {
        std::size_t size;
        std::size_t requested_size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    };

    static constexpr int MAX_ORDER = 10;
    static constexpr int MIN_ORDER = 0;
    static constexpr std::size_t MIN_BLOCK = 128;
    static constexpr std::size_t MAX_BLOCK = (128 * 1024);
    static constexpr std::size_t INIT_SIZE = (32 * 128 * 1024);
    static constexpr std::size_t MAX_REQUEST = 100000000;

    MallocMetadata* free_lists_[MAX_ORDER + 1];
    MallocMetadata* mmap_list_;
    void* base_;
    bool initialized_;

    std::size_t total_buddy_blocks_;
    std::size_t total_mmap_blocks_;
    std::size_t total_mmap_payload_bytes_;

    std::vector<std::string> last_events_;
    std::mutex mutex_;

    int calcRequiredOrder(std::size_t size) const;
    bool initStructure();
    void insertFree(MallocMetadata* block);
    void removeFree(MallocMetadata* block);
    void splitDown(MallocMetadata* block, int req_order);
    void mergeBuddies(MallocMetadata* block);

    bool canMergeForResize(MallocMetadata* block, MallocMetadata* buddy, std::size_t size) const;
    void* tryMergeForResize(void* old_ptr, std::size_t size);

    void* bigAlloc(std::size_t size);
    void bigFree(MallocMetadata* block);
    void* allocateLocked(std::size_t size);
    void deallocateLocked(void* ptr);

    static std::string ptrToHex(const void* ptr);
    static void appendJsonStringEscaped(std::string& out, const std::string& value);
};

} // namespace buddy
