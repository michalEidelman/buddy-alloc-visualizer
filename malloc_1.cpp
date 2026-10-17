#include <unistd.h>

/* Tries to allocate ‘size’ bytes.
 * Return:
 * On success - a pointer to the first allocated byte within the allocated block.
 * On failure -
 *    If ‘size’ is 0 returns NULL.
 *    If ‘size’ is more than 10^8, return NULL.
 *    If sbrk fails, return NULL.
 * */
void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    void* result_sbrk = sbrk(size);
    if(result_sbrk == (void*)(-1)) {
        return NULL;
    }
    return result_sbrk;
}
