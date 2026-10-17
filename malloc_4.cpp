#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define MAX_ORDER 10
#define MIN_ORDER 0
#define MIN_BLOCK 128
#define MAX_BLOCK (128*1024)
#define INIT_SIZE (32*128*1024)

struct MallocMetadata {
    size_t size;
    bool is_free;
    bool is_hugepage;
    MallocMetadata *next;
    MallocMetadata *prev;
};

MallocMetadata *free_lists[MAX_ORDER + 1];
MallocMetadata *mmap_lists = nullptr;
bool initialized = false;
size_t total_buddy_blocks = 0;
size_t total_mmap_blocks = 0;
size_t total_mmap_bytes = 0;

// helper functions
int calc_required_order(size_t size) {
    int k = 0;
    while ((128 << k) < size) {
        k++;
    }
    if (k <= MAX_ORDER) return k;
    return -1;
}

void insert_free(MallocMetadata *to_add) {
    to_add->is_free = true;
    int req = calc_required_order(to_add->size);
    MallocMetadata *curr = free_lists[req];
    if (!curr) {
        to_add->prev = nullptr;
        to_add->next = nullptr;
        free_lists[req] = to_add;
        return;
    }
    if (curr > to_add) {
        to_add->prev = nullptr;
        to_add->next = curr;
        curr->prev = to_add;
        free_lists[req] = to_add;
        return;
    }
    while (curr && curr->next && curr->next < to_add) {
        curr = curr->next;
    }
    to_add->prev = curr;
    if (curr && curr->next != nullptr) {
        to_add->next = curr->next;
        to_add->next->prev = to_add;
    } else {
        to_add->next = nullptr;
    }
    if (curr) curr->next = to_add;
}

void remove_free(MallocMetadata *to_add) {
    to_add->is_free = false;
    int req = calc_required_order(to_add->size);
    MallocMetadata *curr = free_lists[req];
    if (!curr) {
        return;
    }
    //head case
    if (curr == to_add) {
        free_lists[req] = curr->next;
        if (free_lists[req]) {
            free_lists[req]->prev = nullptr;
        }
        curr->next = nullptr;
        return;
    }
    while (curr && curr != to_add) {
        curr = curr->next;
    }
    if (curr) {
        // last node case
        if (!curr->next) {
            curr->prev->next = nullptr;
            curr->prev = nullptr;
        } else {
            curr->prev->next = curr->next;
            curr->next->prev = curr->prev;
            curr->next = nullptr;
            curr->prev = nullptr;
        }
    }
}

void *init_structure() {
    void *curr_brk = sbrk(0);
    size_t addr = (size_t) curr_brk;
    size_t remainder = addr % INIT_SIZE;
    size_t gap = (remainder == 0) ? 0 : (INIT_SIZE - remainder);
    if (gap > 0) {
        if (sbrk(gap) == (void *) (-1)) {
            return nullptr;
        }
    }
    void *base = sbrk(INIT_SIZE);
    if (base == (void *) (-1)) return nullptr;

    for (int i = 0; i < 32; i++) {
        MallocMetadata *block = (MallocMetadata *) ((char *) base + i * MAX_BLOCK);
        block->size = MAX_BLOCK;
        block->is_hugepage = false;

        insert_free(block);
    }
    total_buddy_blocks += 32;
    initialized = true;
    return base;
}

//Challenge 1
void split_down(MallocMetadata *block, int req_order) {
    while (block->size > (128 << req_order)) {
        block->size /= 2;
        auto *buddy = (MallocMetadata *) ((char *) block + block->size);
        buddy->size = block->size;
        insert_free(buddy);
        total_buddy_blocks++;
    }
}

//Challenge 2
void merge_buddies(MallocMetadata *block) {
    auto *buddy = (MallocMetadata *) ((size_t) block ^ block->size);
    while (buddy->is_free && buddy->size == block->size && buddy->size < MAX_BLOCK) {
        remove_free(buddy);
        if ((size_t) block > (size_t) buddy) {
            block = buddy;
        }
        block->size *= 2;
        buddy = (MallocMetadata *) ((size_t) block ^ block->size);
        total_buddy_blocks--;
    }
    insert_free(block);
}

//Challenge 3
void *big_alloc(size_t size, bool is_hugepage) {
    MallocMetadata *temp = mmap_lists;
    MallocMetadata *newBlock = nullptr;
    if (is_hugepage)
    {
        newBlock = (MallocMetadata *) (mmap(nullptr, size + sizeof(MallocMetadata), PROT_READ | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0));
    }
    else
    {
        newBlock = (MallocMetadata *) (mmap(nullptr, size + sizeof(MallocMetadata), PROT_READ | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    }

    if (newBlock == (void *) (-1)) {
        return nullptr;
    }
    newBlock->prev = nullptr;
    newBlock->next = temp;
    newBlock->size = size + sizeof(MallocMetadata);
    newBlock->is_free = false;
    newBlock->is_hugepage = is_hugepage;
    mmap_lists = newBlock;
    if (temp != nullptr) {
        temp->prev = newBlock;
    }
    total_mmap_blocks++;
    total_mmap_bytes += size;
    return (char *) newBlock + sizeof(MallocMetadata);
}
void big_free(MallocMetadata* curr) {
    if (mmap_lists == curr) {
        if (curr->next != nullptr) {
            curr->next->prev=nullptr;
        }
        mmap_lists = curr->next;
    }
    else {

        if (curr->next != nullptr) {
            curr->next->prev = curr->prev;
        }
        curr->prev->next = curr->next;
    }
    total_mmap_blocks--;
    total_mmap_bytes -= (curr->size - sizeof(MallocMetadata));
    munmap(curr, curr->size);
}
bool can_merge_buddies(MallocMetadata *block, MallocMetadata *buddy, size_t size) {
    MallocMetadata* tmp = block;
    size_t tot_size= block->size;
    while (buddy->is_free && buddy->size == tot_size && buddy->size < MAX_BLOCK) {
        if ((size_t) tmp > (size_t) buddy) {
            tmp = buddy;
        }
        tot_size *= 2;
        if (tot_size >= size + sizeof(MallocMetadata)) {
            return true;
        }
        buddy = (MallocMetadata *) ((size_t) tmp ^ tot_size);
    }
    return false;
}
void *try_merge_buddies(void *oldp, size_t size) {
    MallocMetadata *block = (MallocMetadata *) ((char *) oldp - sizeof(MallocMetadata));
    size_t orig_size = block->size;
    auto *buddy = (MallocMetadata *) ((size_t) block ^ block->size);
    if (!can_merge_buddies(block, buddy, size)) return nullptr;
    while (buddy->is_free && buddy->size == block->size && buddy->size < MAX_BLOCK) {
        remove_free(buddy);
        if ((size_t) block > (size_t) buddy) {
            block = buddy;
        }
        block->size *= 2;
        total_buddy_blocks--;
        if (block->size >= size + sizeof(MallocMetadata)) {
            break;
        }
        buddy = (MallocMetadata *) ((size_t) block ^ block->size);
    }
    std::memmove((MallocMetadata *) ((char *) block + sizeof(MallocMetadata)), oldp, orig_size - sizeof(MallocMetadata));
    return (MallocMetadata *) ((char *) block + sizeof(MallocMetadata));
}

void *smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return nullptr;
    }
    if (!initialized) {
        void *init = init_structure();
        if (init == nullptr) return nullptr;
    }
    int req_order = calc_required_order(size + sizeof(MallocMetadata));

    if (req_order == -1) {
        // We need mmap, Challenge 3
        bool is_hugepage = false;
        if (size >= 4*1024*1024) is_hugepage = true;
        return big_alloc(size, is_hugepage);
    }

    int orig_order = req_order;

    while (req_order <= MAX_ORDER && !free_lists[req_order]) {
        req_order++;
    }
    if (req_order > MAX_ORDER) return nullptr;
    MallocMetadata *curr = free_lists[req_order];
    remove_free(curr);
    split_down(curr, orig_order);
    return (char *) curr + sizeof(MallocMetadata);
}


void *scalloc(size_t num, size_t size)
{
    if (size * num == 0 || size * num > 100000000) {
        return nullptr;
    }
    void *res = nullptr;
    if (size >= 2*1024*1024)
    {
        res = big_alloc(size * num, true);

    }else
    {
        res = smalloc(size * num);
    }

    if (res != nullptr) {
        std::memset(res, 0, size * num);
    }
    return res;
}


void sfree(void *p) {
    if (p == nullptr) {
        return;
    }
    MallocMetadata *tmp = (MallocMetadata *) ((char *) p - sizeof(MallocMetadata));
    if (tmp->is_free) return;
    if (tmp->size > MAX_BLOCK) {
        big_free(tmp);
        return;
    }
    merge_buddies(tmp);
}


void *srealloc(void *oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return nullptr;
    }
    MallocMetadata *tmp = nullptr;
    if (oldp != nullptr) {
        tmp = (MallocMetadata *) ((char *) oldp - sizeof(MallocMetadata));
        if (tmp->size > MAX_BLOCK) {
            if (size + sizeof(MallocMetadata) == tmp->size) {
                return oldp;
            }
            void* newBlock = smalloc(size);
            std::memmove(newBlock, oldp, tmp->size - sizeof(MallocMetadata));
            sfree(oldp);
            return newBlock;
        }
        if (size + sizeof(MallocMetadata) <= tmp->size) {
            return oldp;
        }
    }

    void* res = try_merge_buddies(oldp, size);

    if (res == nullptr) {
        res = smalloc(size);
        if (res != nullptr) {
            if (oldp != nullptr) {
                tmp = (MallocMetadata *) ((char *) oldp - sizeof(MallocMetadata));
                std::memmove(res, oldp, tmp->size - sizeof(MallocMetadata));
            }
            sfree(oldp);
        }
    }

    return res;
}

/**
 * Returns the number of allocated blocks in the heap that are currently free.
 ***/
size_t _num_free_blocks() {
    size_t count = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        MallocMetadata *curr = free_lists[i];
        while (curr) {
            count++;
            curr = curr->next;
        }
    }
    return count;
}

/**
* Returns the number of bytes in all allocated blocks in the heap that are currently free,
* excluding the bytes used by the meta-data structs.
 ***/
size_t _num_free_bytes() {
    size_t count = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        MallocMetadata *curr = free_lists[i];
        while (curr) {
            count += (128 << i) - sizeof(MallocMetadata);
            curr = curr->next;
        }
    }
    return count;
}

/**
 * Returns the overall (free and used) number of allocated blocks in the heap.
 ***/
size_t _num_allocated_blocks() {
    return total_buddy_blocks + total_mmap_blocks;
}

/**
* Returns the overall number (free and used) of allocated bytes in the heap, excluding
* the bytes used by the meta-data structs.
 ***/
size_t _num_allocated_bytes() {
    return initialized ? INIT_SIZE - total_buddy_blocks * sizeof(MallocMetadata) + total_mmap_bytes: 0;
}

/**
 * Returns the overall number of meta-data bytes currently in the heap.
 ***/
size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

/**
* Returns the number of bytes of a single meta-data structure in your system.
***/
size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
