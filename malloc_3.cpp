#include "os_malloc.h"
#include "BuddyAllocator.hpp"

void* smalloc(size_t size) {
    return buddy::BuddyAllocator::instance().allocate(size);
}

void* scalloc(size_t num, size_t size) {
    return buddy::BuddyAllocator::instance().callocate(num, size);
}

void sfree(void* p) {
    buddy::BuddyAllocator::instance().deallocate(p);
}

void* srealloc(void* oldp, size_t size) {
    return buddy::BuddyAllocator::instance().reallocate(oldp, size);
}

size_t _num_free_blocks() {
    return buddy::BuddyAllocator::instance().numFreeBlocks();
}

size_t _num_free_bytes() {
    return buddy::BuddyAllocator::instance().numFreeBytes();
}

size_t _num_allocated_blocks() {
    return buddy::BuddyAllocator::instance().numAllocatedBlocks();
}

size_t _num_allocated_bytes() {
    return buddy::BuddyAllocator::instance().numAllocatedBytes();
}

size_t _num_meta_data_bytes() {
    return buddy::BuddyAllocator::instance().numMetadataBytes();
}

size_t _size_meta_data() {
    return buddy::BuddyAllocator::instance().metadataSize();
}
