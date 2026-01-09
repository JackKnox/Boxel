#pragma once

#include "defines.h"

#include "memory.h"

// Represents a single allocation request within a burst allocator.
typedef struct {
	// Address of a pointer that will receive the allocated memory block.
	void** out_pointer;

	// Size of the requested allocation in bytes.
	u64 size;

	// Memory tag used for tracking/debugging allocator usage.
	memory_tag tag;
} burst_allocator_entry;

// Burst allocator which collects multiple allocation requests and
// fulfills them all at once using a single contiguous memory block.
typedef struct {
	// Total size in bytes required to satisfy all recorded allocation entries.
	u64 total_size;

	// Dynamic array of allocation entries describing individual requests.
	burst_allocator_entry* entries;

	// Pointer to the single contiguous memory block backing all allocations.
	void* allocated_block;
} burst_allocator;

// Adds a new allocation request to the burst allocator.
void burst_add_block(burst_allocator* allocator, u64 size, memory_tag tag, void** out_pointer);

// Performs a single allocation large enough to satisfy all recorded allocation requests.
void* burst_allocate_all(burst_allocator* allocator);

// Frees the single allocated memory block and clears all allocation entries, resetting the allocator to an empty state.
void burst_free_all(burst_allocator* allocator);