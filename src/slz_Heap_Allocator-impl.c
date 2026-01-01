/*
This program is an implementation of malloc that i wrote in order to learn about malloc(), mmap() and, the heap
*/



#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>



#define MINIMUM_SIZE 64
#define HEAP_SUCCESS 0
#define HEAP_FAILURE -1
#define NULL_FILE_DESCRIPTOR -1
#define ZERO_OFFSET 0



struct Heap_metadata_t {
    struct Heap_chunk_t* Heap_start_address;
    uint32_t size_avail;
    bool isInit;
};

struct Heap_chunk_t{
    size_t size;
    bool is_inuse;
    struct Heap_chunk_t* heapChunk_nextNode;
};

static struct Heap_metadata_t metadata;




int slz_heap_initialize_impl(struct Heap_metadata_t* heapMetadata) {
    //mmap(2) Kernel memory to our processvoid slz_Heap_deAllocator_Aligned_Sized_impl() {
    long int length_pgsize = sysconf(_SC_PAGESIZE);
    struct Heap_chunk_t* Heap_start_addr = (struct Heap_chunk_t*)mmap(NULL, length_pgsize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, NULL_FILE_DESCRIPTOR, ZERO_OFFSET);
    if (Heap_start_addr == (void* )-1) {
        printf("mmap'ing error\n");
        return HEAP_FAILURE;
    }

    struct Heap_chunk_t* heapHead_chunk = (struct Heap_chunk_t*) Heap_start_addr;
    heapHead_chunk -> size = length_pgsize - sizeof(struct Heap_chunk_t); //calculate size of heap = pgsize - sizeof(heapHead_chunk)
    heapHead_chunk -> is_inuse = false;
    heapHead_chunk -> heapChunk_nextNode = NULL; 

    heapMetadata -> Heap_start_address = Heap_start_addr;
    heapMetadata -> size_avail = length_pgsize - sizeof(struct Heap_chunk_t);
    heapMetadata -> isInit = true;
    return HEAP_SUCCESS;
}



void* slz_Heap_Allocator_impl(size_t size){
    if (metadata.isInit == false){
        printf("error in allocation, heap uninitialized\n");
        return NULL;
    }

    if (size > metadata.size_avail) {
        printf("error in allocation, requested size bigger than available heap size\n");
        return NULL;
    }

    struct Heap_chunk_t* alloc_Chunk = metadata.Heap_start_address;
    while (alloc_Chunk != NULL && (alloc_Chunk -> size < size || alloc_Chunk -> is_inuse == true)) {
        alloc_Chunk = alloc_Chunk -> heapChunk_nextNode;
    }

    //searches for a chunk that isnt in use and has a at least equal size to the requested size.
    if (alloc_Chunk == NULL) {
        return NULL;
    }

    //if chunk size greater than size: 'chop' chunk, give size in chunk and 'chunkify' the rest of the chunk
    else if (size + MINIMUM_SIZE < alloc_Chunk -> size) {
        //choppedChunk should start at address of alloc_Chunk's data + size because we are chopping it to inuse and not inuse chunks
        struct Heap_chunk_t* choppedChunk = (struct Heap_chunk_t*)((char*)alloc_Chunk + sizeof(struct Heap_chunk_t) + size);
        choppedChunk -> size = alloc_Chunk -> size - size - sizeof(struct Heap_chunk_t);//choppedChunk's size is equal to the non chopped's size - requested size - metadata size
        choppedChunk -> is_inuse = false;//flag chopped chunk as unused
        choppedChunk -> heapChunk_nextNode = alloc_Chunk -> heapChunk_nextNode;//make choppedChunk point to the same chunk it pointed to before 'choppification'

        //work on the chunk we will allocate
        alloc_Chunk -> size = size;
        alloc_Chunk -> is_inuse = true;
        alloc_Chunk -> heapChunk_nextNode = choppedChunk;

        //return the data
        metadata.size_avail = metadata.size_avail - (size + sizeof(struct Heap_chunk_t)); //available space is smaller by size allocated and chunk metadata size
        return (void*)((char*)alloc_Chunk + sizeof(struct Heap_chunk_t)); //equivalant to returning: (void*)(alloc_Chunk + 1) but more explicit

    }
    //if size = chunk size or size = some amount in between (chunksize - MINIMUM_SIZE) and chunksize
    else if (size <= alloc_Chunk -> size) {
        alloc_Chunk -> is_inuse = true;
        metadata.size_avail = metadata.size_avail - (size + sizeof(struct Heap_chunk_t)); //available space is smaller by size allocated and chunk metadata size
        return (void* )((char* )alloc_Chunk + sizeof(struct Heap_chunk_t)); //equivalant to returning: (void*)(alloc_Chunk + 1) but more explicit
    }

    return NULL;
}
//malloc-implementation



void* slz_Heap_Allocator_init_impl(size_t numElements, size_t sizeElements) {
    size_t sizeAllocation = numElements * sizeElements;
    if (sizeAllocation == -1) {
        printf("Integer overflow in slz_Heap_Allocator_init_impl\n");
        return NULL;
    }

    void* allocationPtr = slz_Heap_Allocator_impl(sizeAllocation);
    if (allocationPtr == NULL) {
        printf("allocation failure in slz_Heap_Allocator_init_impl");
        return NULL;
    }

    allocationPtr = memset(allocationPtr, 0x00, sizeAllocation);
    return allocationPtr;
}
//calloc-implementation





//after freeing, check if next chunk is free if it is coalesce and look ahead for more free chunks until u find a chunk in use or NULL
//!!I SHOULD REALLY CHECK THE LOGIC ON THIS THING, MIGHT NEVER DO BACKWARDS DEFRAGMENTATION/COALESCENCE!!
int slz_Heap_deAllocator_impl(void* deAllocation_addr) {

    if (metadata.isInit == false) {
        printf("error deallocating memory, heap not initialized\n");
        return HEAP_FAILURE;
    }

    struct Heap_chunk_t* deAllocation_chunk = (struct Heap_chunk_t*)((char*)deAllocation_addr - sizeof(struct Heap_chunk_t));//deAllocation_addr points to data, we need a pointer to the chunk metadata before the chunk itself
    deAllocation_chunk -> is_inuse = false;
    metadata.size_avail += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;


    //coalescing with next chunk if possible
    while (deAllocation_chunk -> heapChunk_nextNode != NULL && deAllocation_chunk -> heapChunk_nextNode -> is_inuse == false) {
        //if next chunk is free and not null, coalesce
        deAllocation_chunk -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> heapChunk_nextNode -> size;
        deAllocation_chunk -> heapChunk_nextNode = deAllocation_chunk -> heapChunk_nextNode -> heapChunk_nextNode;
    }

    //if at top of heap dont coalesce backwards
    if (deAllocation_chunk == metadata.Heap_start_address) {
        return HEAP_SUCCESS;
    }
    //coalescing with previous chunk if possible
    //look from metadata.Heap_start_address to deAllocation_chunk and see if we can coalesce from the back
    struct Heap_chunk_t* prevNode_coalescence = metadata.Heap_start_address;
    if (prevNode_coalescence == NULL) {
        printf("Catastrophic heap initialization Failure");
        return HEAP_FAILURE;
    }

    while (prevNode_coalescence -> heapChunk_nextNode != NULL && prevNode_coalescence -> heapChunk_nextNode != deAllocation_chunk) {
        prevNode_coalescence = prevNode_coalescence -> heapChunk_nextNode;
    }
    //this finds the node behind deAllocation_chunk
    //we should check if it actually finds it
    if (prevNode_coalescence == NULL) {
        printf("Failed at finding previous node to deAllocation_chunk");
        return HEAP_FAILURE;
    }

    if (prevNode_coalescence -> is_inuse == false) {
        if (deAllocation_chunk -> heapChunk_nextNode != NULL) {
            prevNode_coalescence -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;
            prevNode_coalescence -> heapChunk_nextNode = deAllocation_chunk -> heapChunk_nextNode;
        }
        else {
            prevNode_coalescence -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;
            prevNode_coalescence -> heapChunk_nextNode = NULL;
        }
    }
    //this coalesces the previous node in the free list

    return HEAP_SUCCESS;
}
//free-implementation



size_t slz_Heap_deAllocator_Sized_impl(void* deAllocation_addr, size_t size) {

    if (metadata.isInit == false) {
        printf("error deallocating memory, heap not initialized\n");
        return HEAP_FAILURE;
    }

    //litterly just slz_Heap_deAllocator_impl but returns the size passed to it
    struct Heap_chunk_t* deAllocation_chunk = (struct Heap_chunk_t*)((char*)deAllocation_addr - sizeof(struct Heap_chunk_t));//deAllocation_addr points to data we need pointer to chunk metadata before the chunk
    deAllocation_chunk -> is_inuse = false;
    metadata.size_avail += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;

    //coalescing with next chunk if possible
    while (deAllocation_chunk -> heapChunk_nextNode != NULL && deAllocation_chunk -> heapChunk_nextNode -> is_inuse == false) {//in this while loop we find deAllocation_chunk=deAllocation_chunk->heapChunk_nextNode
        //if next chunk is free and not null, coalesce
        deAllocation_chunk -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> heapChunk_nextNode -> size;
        deAllocation_chunk -> heapChunk_nextNode = deAllocation_chunk -> heapChunk_nextNode -> heapChunk_nextNode;
    }

    //if at top of the list: dont coalesce backwards
    if (deAllocation_chunk == metadata.Heap_start_address) {
        return size;
    }

    //coalescing with previous chunk if possible
    //look from metadata.Heap_start_address to deAllocation_chunk and see if we can coalesce from the back
    struct Heap_chunk_t* prevNode_coalescence = metadata.Heap_start_address;
    if (prevNode_coalescence == NULL) {
        printf("Catastrophic heap initialization Failure");
        return HEAP_FAILURE;
    }

    while (prevNode_coalescence -> heapChunk_nextNode != NULL && prevNode_coalescence -> heapChunk_nextNode != deAllocation_chunk) {
        prevNode_coalescence = prevNode_coalescence -> heapChunk_nextNode;
    }
    //this finds the node behind deAllocation_chunk

    //we should check if it actually finds the node behind deAllocation_chunk
    if (prevNode_coalescence == NULL) {
        printf("Failed at finding previous node to deAllocation_chunk");
        return HEAP_FAILURE;
    }
    if (prevNode_coalescence -> heapChunk_nextNode == deAllocation_chunk && deAllocation_chunk != NULL) {
        if (prevNode_coalescence -> is_inuse == false) {
            //check if the chunk after deAllocation_chunk is null to see if we need to set prev's next chunk to null or to the next chunk after deAllocation_chunk
            if (deAllocation_chunk -> heapChunk_nextNode != NULL) {
                prevNode_coalescence -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;
                prevNode_coalescence -> heapChunk_nextNode = deAllocation_chunk -> heapChunk_nextNode;
            }
            else {
                prevNode_coalescence -> size += sizeof(struct Heap_chunk_t) + deAllocation_chunk -> size;
                prevNode_coalescence -> heapChunk_nextNode = NULL;
            }
        }
    //this coalesces the previous node in the free list
    }
    return size;
}




void* slz_Heap_reAllocator_impl(void* Allocation_address, size_t newSize){
    
    if (Allocation_address == NULL) {
        return slz_Heap_Allocator_impl(newSize);
    }

    if (newSize == 0) {
        int deAllocation_Status = slz_Heap_deAllocator_impl(Allocation_address);
        return NULL;
    }
    
    struct Heap_chunk_t* reAlloc_chunk = (struct Heap_chunk_t*)((char*)Allocation_address - sizeof(struct Heap_chunk_t));
    if (newSize < reAlloc_chunk -> size) {
        if (reAlloc_chunk -> size - newSize > MINIMUM_SIZE + sizeof(struct Heap_chunk_t)) {
            size_t remainder_Size = reAlloc_chunk -> size - newSize - sizeof(struct Heap_chunk_t);//calculates the address offset from reAlloc_chunk to new chunk
            struct Heap_chunk_t* remainder_Chunk = (struct Heap_chunk_t*)((char*)reAlloc_chunk + sizeof(struct Heap_chunk_t) + newSize);
            remainder_Chunk -> is_inuse = false;
            reAlloc_chunk -> size = newSize;

            while (remainder_Chunk -> heapChunk_nextNode != NULL && remainder_Chunk -> heapChunk_nextNode -> is_inuse == false) {
                //if next chunk is free and not null, coalesce
                remainder_Chunk -> size += sizeof(struct Heap_chunk_t) + remainder_Chunk -> heapChunk_nextNode -> size; //gets the next chunk's size
                remainder_Chunk -> heapChunk_nextNode = remainder_Chunk -> heapChunk_nextNode -> heapChunk_nextNode; //removes next chunk from the free list; Completes the Coalescence
            }
            return Allocation_address;
        }
    }

    if (reAlloc_chunk -> size < newSize) {
        if (reAlloc_chunk -> heapChunk_nextNode != NULL 
            && (newSize <= reAlloc_chunk -> size + sizeof(struct Heap_chunk_t) + reAlloc_chunk -> heapChunk_nextNode -> size) 
            && (reAlloc_chunk -> heapChunk_nextNode -> is_inuse == false)) {
            
                reAlloc_chunk -> size += sizeof(struct Heap_chunk_t) + reAlloc_chunk -> heapChunk_nextNode -> size;
            reAlloc_chunk -> heapChunk_nextNode = reAlloc_chunk -> heapChunk_nextNode -> heapChunk_nextNode;
            if (newSize + MINIMUM_SIZE < reAlloc_chunk -> size) { //if the new chunk's size is too big for newSize then we chop the new chunk into two chunks, one that is newSize'd and one that is the other part
                size_t remainingSize = reAlloc_chunk->size - newSize - sizeof(struct Heap_chunk_t);
                struct Heap_chunk_t* choppedChunk = (struct Heap_chunk_t*)(char*)reAlloc_chunk + sizeof(struct Heap_chunk_t) + newSize;

                choppedChunk -> size = remainingSize;
                choppedChunk -> is_inuse = false;
                choppedChunk -> heapChunk_nextNode = reAlloc_chunk -> heapChunk_nextNode;
                
                reAlloc_chunk -> size = newSize;
                reAlloc_chunk -> heapChunk_nextNode = choppedChunk;
                return Allocation_address;
            }
        }

        if (reAlloc_chunk -> heapChunk_nextNode != NULL && (reAlloc_chunk -> heapChunk_nextNode -> is_inuse == true | reAlloc_chunk -> heapChunk_nextNode -> size < newSize - reAlloc_chunk -> size)) {
            void* allocation_ptr = slz_Heap_Allocator_impl(newSize);
            if (allocation_ptr == NULL) {
                printf("Failure calling malloc in reAlloc function!\n");
                return NULL;
            }
            memcpy(allocation_ptr,Allocation_address, reAlloc_chunk -> size);
            int free_status = slz_Heap_deAllocator_impl(Allocation_address);
            if (free_status == HEAP_FAILURE) {
                printf("failure calling heap_deallocator in realloc\n");
                return NULL;
            }

            return allocation_ptr;
        }
    }

    return NULL;
}
//realloc implementation


int main(void) {
    metadata.isInit = false;
    metadata.Heap_start_address = NULL;
    metadata.size_avail = 0;
    //these lines can be commented out but main() is a test so optimization isnt neccasary, and this is just good validation
    //initialize the heap using slz_heap_initialize_impl: int initHeap = slz_heap_initialize_impl(heapMetadataVariable);... if(initHeap == errcode){handle error...}
    int initHeap = slz_heap_initialize_impl(&metadata);
    if (initHeap == HEAP_FAILURE) {
        printf("Failure in heap initialization\n");
    }
    else {
        printf("success\n");
    }

    printf("initializing test 1: print a test_Allocation pointer\n");
    void* test_Allocation = slz_Heap_Allocator_impl(8 * sizeof(uint32_t));
    if (test_Allocation == (void*)HEAP_FAILURE) {
        printf("test 1 error");
    }
    printf("test Allocation pointer = %p\n",test_Allocation); 
    
    printf("initializing test 2: making an int array on the heap\n");
    uint32_t arrayLength = 56;
    int* testArray = (int*)slz_Heap_Allocator_impl(arrayLength * sizeof(int));
    if (testArray == (int*)HEAP_FAILURE) {
        printf("test 2 error\n");
    }
    printf("%p\n", (void*) testArray);

    uint32_t i = 0;
    while (i < arrayLength) {
        testArray[i] = i;
        printf("%d\n", testArray[i]);
        printf("%p\n", &testArray[i]);
        ++i;
    }
    
    printf("initialization test 3: deallocating test_Allocation and arr\n"); 
    uint32_t deallocation_status = slz_Heap_deAllocator_impl(test_Allocation);
    if (deallocation_status == HEAP_FAILURE) {
        printf("slz_Heap_deAllocator_impl returned HEAP_FAILURE(-1)\n");
        return HEAP_FAILURE;
    }
    printf("free'd: %p\n", test_Allocation);
    printf("success deallocating test_Allocation\n");
    

    size_t deallocation_size = slz_Heap_deAllocator_Sized_impl((void*)testArray, arrayLength * sizeof(int));
    printf("free'd: %p\n", (void*)testArray);
    printf("success deallocating testArray\n");


    printf("initializing test 4: allocating and deallocating a zero'd memory array\n");
    size_t numElements = 9;
    size_t sizeElements = sizeof(int);
    int* zeroArray = (int*)slz_Heap_Allocator_init_impl(numElements, sizeElements);

    uint32_t Idx = 0;
    while (Idx < numElements - 1) {
        printf("%d\n", zeroArray[Idx]);
        Idx++;
    }
    uint32_t zeroArraydeAlloc = slz_Heap_deAllocator_impl((void*)zeroArray);
    if (zeroArraydeAlloc == HEAP_FAILURE) {
        printf("Failure deallocating zeroArray\n");
        return HEAP_FAILURE;
    }
    printf("Success deallocating zeroArray\n");

    printf("initializing test 5: allocating a array and reallocating it and then deallocating it\n");
    int* reallocTest = (int*)slz_Heap_Allocator_impl(2 * sizeof(int));
    for (int i = 0; i < 2; ++i) {
        reallocTest[i] = i;
        printf("%d\n", reallocTest[i]);
    }
    reallocTest = (int*)slz_Heap_reAllocator_impl(reallocTest, 8 * sizeof(int));

    for (int j = 0; j < 8; ++j) {
        if (j > 1) {
            reallocTest[j] = j;
        }
        printf("%d\n", reallocTest[j]);
    }

    int free_status = slz_Heap_deAllocator_impl((void*)reallocTest);
    if (free_status == HEAP_FAILURE) {
        printf("failure deallocating reallocTest array\n");
        return HEAP_FAILURE;
    }
    printf("success in test 5\n");
    
    
    printf("Compiled succesfully!\n");
    return HEAP_SUCCESS;

}




