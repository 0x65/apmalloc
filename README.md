Writing Your Own Memory Allocator
=================================

Memory allocators are pretty important pieces of software that are responsible for dynamically managing accessible memory. A good memory allocator is one that minimizes the time spent requesting/releasing memory while also minimizing the amount of wasted space - two goals that are actually at odds with each other. There are a good deal of mature memory allocators (dlmalloc, ptmalloc, jemalloc, tcmalloc, just to name a few), so why would you ever want to build your own?

One reason is that the aforementioned allocators are typically general use allocators. If you are developing on a specific platform (say, embedded devices), these may be way too bulky for you. A specific-purpose allocator could provide a lot of benefits. For example, if your allocation requests are all for a fixed size, your allocator probably doesn't have to worry about spending time to coalesce and split blocks. A lot of allocators do have good options for tuning, but even then, writing your own memory allocator can serve as a good learning tool!


A High-Level Overview
---------------------

Memory allocators work by requesting some block of memory from the operating system (typically larger than whatever was requested, to minimize the amount of expensive system calls) and keeping track which parts of this memory have already been allocated and which parts have been freed. Most memory allocators treat the block of memory as a (not necessarily contiguous) list of free and allocated blocks, which make linked lists the most ubiquitous data structure in implementations. To service an allocation request, the memory allocator scans its list of memory blocks looking for a free block at least as big as the size that was requested, does some necessary bookkeeping, and returns that address. If there's no free block big enough, it has to request more memory from the operating system. The method by which it scans and chooses a free block is the central algorithm of the memory allocator, and the method by which the allocator frees memory is typically dependent on the algorithm used and the breadth of the bookkeeping information the allocater keeps. One thing that the free call should do, however, is *coalesce* the free blocks - that is, combine adjacent free blocks in the memory list to minimize fragmentation, which wastes space.

As hinted, allocators keep some amount of information on each block of memory. This information is kept in a struct called the header and is located right before the actual memory returned by malloc(). It allows the allocator to quickly obtain information like the size of the block and whether or not it is in use simply by subtracting sizeof(header) from a pointer to memory. Memory allocators try to keep the header small (so that the overhead of requesting a small block is not enormous) but informative (so that malloc/free can work quickly). Some allocators also include a small footer at the end of each block which typically only contains the block's size - this allows for finding the size of the block to the left of some other block trivial, and depending on the algorithm can speed up things like coalescing blocks.


Methods of Allocating Memory
----------------------------

As mentioned, the allocator must at times acquire a block of memory with which it can work. On a UNIX kernel, there are three principal ways of doing so:

    - Use memory on the stack. Typically this is done with alloca(3). This is a machine- and compiler-specific function that is not POSIX-compliant. One benefit (or detriment, depending on who you ask) is that the implementation for free() is trivial - when the stack frame is destroyed, the memory is automatically reclaimed. However, the stack is typically smaller than what is useful, and with alloca(3), program behavior is undefined if a stack overflow occurs. It is unsuitable for a general-purpose memory allocator.

    - Use memory on the heap. The heap is a dynamic part of the data segment of the virtual address space of every process, and its limit (or break) can be set with brk(2) and sbrk(2) system calls. Note that keeping track of the heap using the break allows for very easy allocation (simply increase it), but giving memory back must be done in a LIFO order. Traditionally this has been the method employed by memory allocators, but evidence suggests that this method is more prone to race conditions and increased fragmentation.

    - Anonymous mapping of the process's virtual memory using the mmap(2) system call. While mmap(2) is POSIX-compliant, anonymous mappings are not. Nonetheless, more and more memory allocators are beginning to use this method, which is potentially simpler to use, allows for easy deallocation using munmap(2), and has potentially better security and efficiency. One drawback mmap(2) has is that it allocates memory only in page-sized blocks.


Overview of Algorithms and Structures
-------------------------------------

The most obvious algorithm for a memory allocator would just be to keep one big list of sequential memory blocks, with some kind of header indicating what the size of the block is and whether or not it is free (actually, the latter can be encoded in the last bit of the former, if all the sizes are a multiple of 2). The good thing about this method is that writing free() is very simple - just set the free flag to true! Coalescing makes things slightly more tricky because we don't know the size of the block on the left, so we would have to add a footer to each block indicating its size as well. Even so, the call to free() would take constant time. The problem is allocating memory, though - in the worst case, when most memory blocks are already full, we'd have to scan through the entire list, which is entirely too slow.

There are a couple method of actually choosing the block we return from malloc(). A simple method is first fit - just choose the first block we see of size m > n. The problem is that this method is more prone to fragmentation. On the other hand we have best fit - choose the block with a size closest to the request. This leads to less fragmentation, but we'd have to do a lot more scanning to find the block. We also have next fit, which starts subsequent scans from the point where the last scan left off. It's slightly faster than first fit since it avoids scanning some already in-use blocks, but requires something like a circular linked list.

What if instead of keeping all blocks in a list, we kept only the ones that were free? Well, the list wouldn't be of contiguous blocks anymore, so our header would have to include pointers to both the previous and next block in addition to the size. Malloc() is going to be a lot faster, though, because we only have to scan through the free blocks. How do we free a block using this method? A simple way would just to be to place it at the head of the free list - simple and fast to implement, though studies on memory allocators have shown that this tends to result in more fragmentation (how would you coalesce blocks in such a scenario?). An alternative that reduces fragmentation is address order policy - namely, place the block so that the address of it is greater than the address of the previous block but less than the address of the next block. Naturally, scanning the list for such a position takes more time.

Let's approach it from a different angle. We note that having a free list with first fit is great for speed, but horrible for fragmentation. Best fit would improve fragmentation, but make scanning much too slow. Is there a way to approximate a best fit? It turns out, sort of. Best fit works by matching a request of size n with a block whose size is closest to n (while being larger). So, if we kept a group of free lists of certain sizes, we could match a request to an approximate size, and use first fit on that list to get a block quickly. This is a good idea, and a lot of memory allocators use a variant of this strategy.


Implementation
--------------
