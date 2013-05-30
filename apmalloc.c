#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define PAGE_SIZE (4096)
#define MAX_BINS (12)   // 2^12 = 4096

#if INTPTR_MAX == INT32_MAX
    #define ALIGN(x) (((((x)-1)>>2)<<2)+4)
#elif INTPTR_MAX == INT64_MAX
    #define ALIGN(x) (((((x)-1)>>3)<<3)+8)
#else
    #error
#endif

#define MMAP(n) (mmap(0, (size_t)(n), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0))

typedef struct _header {
    struct _header *prev, *next;
    size_t size;
} header_t;

// the n'th (0 <= n < MAX_BINS) bin is defined as holding free blocks from 2^n+1 up to 2^(n+1) in size
// TODO: because of header size, first couple of bins will always be unused
static header_t *free_list[MAX_BINS];

static inline unsigned int bin_index(unsigned int n) {
    if (n <= 2) return 0;
    return ((sizeof(unsigned int) * CHAR_BIT) - __builtin_clz(n-1)) - 1;
}

static inline unsigned int round_to_next_page(unsigned int n) {
    unsigned int x = PAGE_SIZE - 1;
    return (n+x)&~x;
}






void *apmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t request_size = ALIGN(size + sizeof(header_t));

    // if it's more than our last bin's capacity, just mmap it
    if (request_size >= (1 << MAX_BINS)) {
        header_t *header;
        request_size = round_to_next_page(request_size);

        void *ptr = MMAP(request_size);
        if (ptr == MAP_FAILED) return NULL;

        header = (header_t*)ptr;
        header->size = request_size;
        return (void*)(header + 1);
    }

    unsigned int request_bin = bin_index(request_size);

    return NULL;
}

void apfree(void *ptr) {
    if (ptr == NULL) return;

    header_t *header = (header_t*)ptr - 1;

    // if it's bigger than our last bin's capacity, we used mmap
    if (header->size >= (1 << MAX_BINS)) {
        munmap(header, header->size);
        return;
    }

}

#include <stdio.h>

int main() {
    header_t f;
    int* blah = apmalloc(4096);
    *blah = 1000;
    apfree(blah);
    *blah = 1020;
}
