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

static void insert_free_block(header_t *block) {
    unsigned int index = bin_index(block->size);
    header_t *previous = NULL, *current = free_list[index];

    while (current != NULL && current < block) {
        previous = current;
        current = current->next;
    }

    if (previous) {
        previous->next = block;
        block->prev = previous;
    } else {
        free_list[index] = block;
    }

    if (current) current->prev = block;
    block->next = current;

    // TODO: coalesce here
}

void *apmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t request_size = ALIGN(size + sizeof(header_t));

    // if it's more than our last bin's capacity, just mmap it
    if (request_size >= (1 << MAX_BINS)) {
        request_size = round_to_next_page(request_size);

        void *ptr = MMAP(request_size);
        if (ptr == MAP_FAILED) return NULL;

        header_t *header = (header_t*)ptr;
        header->size = request_size;
        return (void*)(header + 1);
    }

    // look for free blocks in our lists
    for (unsigned int request_bin = bin_index(request_size); request_bin < MAX_BINS; request_bin++) {
        header_t *candidate = free_list[request_bin];

        while (candidate != NULL) {
            // we found a suitable block
            if (candidate->size >= request_size) {
                // take this block out of the free list
                if (candidate->prev) candidate->prev->next = candidate->next;
                if (candidate->next) candidate->next->prev = candidate->prev;
                if (!(candidate->prev || candidate->next)) free_list[request_bin] = NULL;

                // if there's a remainder, add it back to the appropriate list
                if (candidate->size > request_size) {
                    header_t *remainder = (header_t*)((char*)candidate + request_size);
                    remainder->size = candidate->size - request_size;
                    insert_free_block(remainder);

                    candidate->size = request_size;
                }

                return (void*)(candidate + 1);
            }

            candidate = candidate->next;
        }
    }

    // no block big enough found, request and split a big chunk
    void *ptr = MMAP(PAGE_SIZE);
    if (ptr == MAP_FAILED) return NULL;

    header_t *remainder = (header_t*)((char*)ptr + request_size);
    remainder->size = PAGE_SIZE - request_size;
    insert_free_block(remainder);

    header_t *header = (header_t*)ptr;
    header->size = request_size;
    return (void*)(header + 1);
}

void apfree(void *ptr) {
    if (ptr == NULL) return;

    header_t *header = (header_t*)ptr - 1;

    // if it's bigger than our last bin's capacity, we used mmap
    if (header->size >= (1 << MAX_BINS)) munmap(header, header->size);
    // otherwise just insert it into our free lists
    else insert_free_block(header);
}








void print_list() {
    for (int i = 0; i < MAX_BINS; i++) {
        printf("List %d:", i);
        if (free_list[i] == NULL) printf("<empty>");
        else {
            header_t *h = free_list[i];
            while (h != NULL) {
                printf("%x (%d) ; ", h, h->size);
                h = h->next;
            }
        }
        printf("\n");
    }
}


#include <stdio.h>

int main() {
    printf("Start:\n");
    print_list();
    printf("--------------------------------------------------\n");
    printf("Malloc 80 blocks\n");
    int* blah = apmalloc(56);
    print_list();
    printf("--------------------------------------------------\n");
    printf("Free 80 blocks\n");
    apfree(blah);
    print_list();
    printf("--------------------------------------------------\n");
    printf("Malloc 40 bytes\n");
    int* blah2 = apmalloc(10);
    print_list();
    printf("--------------------------------------------------\n");
    printf("Free 40 bytes\n");
    apfree(blah2);
    print_list();
    printf("--------------------------------------------------\n");
}
