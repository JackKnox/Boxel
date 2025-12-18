#pragma once

#include "defines.h"

// A data structure to be used for dynamic memory allocation. Tracks free ranges of memory.
typedef struct freelist {
    // The internal memory of the freelist.
    void* memory;

    // Number of bytes allocated by freelist.
    u64 capacity;

    // Size of region used by freelist within managed buffer.
    u64 size;

    // Memory tag of internal memory in freelist.
    memory_tag tag;
} freelist;

// Creates a new freelist.
void freelist_create(u64 start_size, memory_tag tag, freelist* out_list);

// Safely checks if data at pointer is freelist or is freelist and capacity == 0.
b8 freelist_empty(freelist* list);

// Destroys the provided list.
void freelist_destroy(freelist* list);

// Gets size of region used by freelist within managed buffer in bytes.
u64 freelist_size(freelist* list);

// Gets number of bytes allocated by freelist.
u64 freelist_capacity(freelist* list);

// Resizes freelist to size and cuts off memory if new size is smaller than current size.
void freelist_resize(freelist* list, u64 new_size);

// Resets freelist for reusing memory within.
void freelist_reset(freelist* list, b8 zero_memory, b8 free_memory);

// Pushes free range of memory onto list.
void* freelist_push(freelist* list, u64 block_size, void* memory);

// Gets index'th block and returns pointer, use sparingly or use freelist_next_block.
void* freelist_get(freelist* list, u64 index);

// Used for iteration and while loop.
b8 freelist_next_block(freelist* list, u8** cursor);