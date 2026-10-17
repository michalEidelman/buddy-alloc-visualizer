#include <unistd.h>
#include <cstring>

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* head = nullptr;
/**
● Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
  found.
● Return value:
  i. Success – returns pointer to the first byte in the allocated block (excluding the meta-data of
     course)
  ii. Failure –
     a. If size is 0 returns NULL.
     b. If ‘size’ is more than 10^8, return NULL.
     c. If sbrk fails in allocating the needed space, return NULL
 ***/
void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }

    MallocMetadata *curr = head;
    while(curr != nullptr && ((!curr->is_free) || curr->size < size)) {
        curr = curr->next;
    }
    if (curr != nullptr) {
        curr->is_free = false;
        return (char*)curr + sizeof(MallocMetadata);
    }

    void* result_sbrk = sbrk(size + sizeof(MallocMetadata));
    if(result_sbrk == (void*)(-1)) {
        return NULL;
    }
    auto tmp = static_cast<MallocMetadata*>(result_sbrk);
    tmp->is_free = false;
    tmp->size = size;

    if(head == nullptr) {
        head = tmp;
        head->next = nullptr;
        head->prev = nullptr;
    } else {
        MallocMetadata* curr = head;
        while (curr && curr->next) {
            curr = curr->next;
        }
        tmp->prev = curr;
        tmp->next = nullptr;
        curr->next = tmp;
    }

    return (char*)result_sbrk + sizeof(MallocMetadata);
}

/**
● Searches for a free block of at least ‘num’ elements, each ‘size’ bytes that are all set to 0
  or allocates if none are found. In other words, find/allocate size * num bytes and set all
  bytes to 0.
● Return value:
  i. Success - returns pointer to the first byte in the allocated block.
  ii. Failure –
     a. If size or num is 0 returns NULL.
     b. If ‘size * num’ is more than 10^8, return NULL.
     c. If sbrk fails in allocating the needed space, return NULL.
 ***/
void* scalloc(size_t num, size_t size) {

    if (size * num == 0 || size * num > 100000000) {
        return NULL;
    }
    void* res = smalloc(size * num);
    if (res != NULL) {
        std::memset(res, 0, size*num);
    }
    return res;
}

/**
● Releases the usage of the block that starts with the pointer ‘p’.
● If ‘p’ is NULL or already released, simply returns.
● Presume that all pointers ‘p’ truly points to the beginning of an allocated block.
 ***/
void sfree(void* p) {
    if(p == NULL) {
        return;
    }
    MallocMetadata* tmp = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    if(tmp->is_free) return;
    tmp->is_free = true;
}

/**
● If ‘size’ is smaller than or equal to the current block’s size, reuses the same block.
   Otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the
   new allocated space and frees the oldp.
● Return value:
  i. Success –
     a. Returns pointer to the first byte in the (newly) allocated space.
     b. If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it.
  ii. Failure –
     a. If size is 0 returns NULL.
     b. If ‘size’ if more than 10^8, return NULL.
     c. If sbrk fails in allocating the needed space, return NULL.
     d. Do not free ‘oldp’ if srealloc() fails.
 ***/
void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    MallocMetadata* tmp = nullptr;
    if(oldp != NULL) {
        tmp = (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata));
        if(size <= tmp->size) {
            return oldp;
        }
    }
    void* res = smalloc(size);
    if(res != NULL) {
        if(oldp != NULL) {
            std::memmove(res, oldp, tmp->size);
        }
        sfree(oldp);
    }
    return res;
}

/**
 * Returns the number of allocated blocks in the heap that are currently free.
 ***/
size_t _num_free_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr) {
        count += curr->is_free ? 1 : 0;
        curr = curr->next;
    }
    return count;
}

/**
* Returns the number of bytes in all allocated blocks in the heap that are currently free,
* excluding the bytes used by the meta-data structs.
 ***/
size_t _num_free_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr) {
        count += curr->is_free ? curr->size : 0;
        curr = curr->next;
    }
    return count;
}

/**
 * Returns the overall (free and used) number of allocated blocks in the heap.
 ***/
size_t _num_allocated_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr) {
        count++;
        curr = curr->next;
    }
    return count;
}

/**
* Returns the overall number (free and used) of allocated bytes in the heap, excluding
* the bytes used by the meta-data structs.
 ***/
size_t _num_allocated_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr) {
        count += curr->size;
        curr = curr->next;
    }
    return count;
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

