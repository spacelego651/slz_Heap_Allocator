# Custom Malloc Implementation

A learning-focused implementation of dynamic memory allocation functions in C, built from scratch using `mmap()` and heap management techniques.

## Overview

This project implements a custom memory allocator to understand heap management internals, memory allocation strategies, and system calls. It provides alternatives to standard library functions (`malloc`, `calloc`, `free`, `realloc`) for educational purposes.

## Core Functions

### `slz_heap_initialize_impl()`
Initializes the heap using `mmap()` to allocate one page of memory from the kernel.

### `slz_Heap_Allocator_impl(size_t size)`
Allocates a block of memory using a first-fit strategy.

### `slz_Heap_Allocator_init_impl(size_t numElements, size_t sizeElements)`
Allocates and zero-initializes memory (calloc equivalent).

### `slz_Heap_deAllocator_impl(void* deAllocation_addr)`
Frees memory and coalesces adjacent free chunks to reduce fragmentation.

### `slz_Heap_reAllocator_impl(void* Allocation_address, size_t newSize)`
Resizes an existing allocation, attempting in-place resizing when possible.

## Memory Management Strategy

- **First-Fit Allocation**: Searches for the first suitable free chunk
- **Chunk Splitting**: Divides large chunks when smaller allocations are requested
- **Automatic Coalescing**: Merges adjacent free chunks during deallocation
- **Minimum Chunk Size**: 64-byte minimum to prevent excessive fragmentation

## Data Structures

**Heap Metadata**: Tracks heap start address, available size, and initialization status

**Heap Chunk**: Linked list nodes containing size, in-use flag, and next chunk pointer


The program includes a basic test routine demonstrating allocator functionality.


## Limitations

- Single-page heap (typically 4KB)
- No thread safety
- Simple first-fit strategy (not performance-optimized)
- Educational implementation - not suitable for production

## Learning Outcomes

- System calls for memory management (`mmap`, `sysconf`)
- Heap data structure design
- Memory fragmentation and coalescing
- Pointer arithmetic in C
- Metadata management for allocators

## License

Educational code for learning purposes.
