#include "defines.h"
#include "allocators.h"

#include "platform/platform.h"
#include "utils/darray.h"

void burst_add_block(burst_allocator* allocator, u64 size, memory_tag tag, void** out_pointer) {
	BX_ASSERT(allocator != NULL && size > 0 && tag < MEMORY_TAG_MAX_TAGS && out_pointer != NULL && "Invalid arguments passed to burst_add_block");
	if (!allocator->entries)
		allocator->entries = darray_create(burst_allocator_entry, MEMORY_TAG_CORE);

	burst_allocator_entry entry = { 0 };
	entry.out_pointer = out_pointer;
	entry.size = size;
	entry.tag = tag;

	allocator->entries = _darray_push(allocator->entries, &entry);
	allocator->total_size += size;
}

void* burst_allocate_all(burst_allocator* allocator) {
	BX_ASSERT(allocator != NULL && "Invalid arguments passed to burst_allocate_all");
	allocator->allocated_block = platform_allocate(allocator->total_size, FALSE);
	platform_set_memory(allocator->allocated_block, 0, allocator->total_size);

	u64 offset = 0;

	for (u32 i = 0; i < darray_length(allocator->entries); ++i) {
		burst_allocator_entry* entry = &allocator->entries[i];
		*entry->out_pointer = (u8*)allocator->allocated_block + offset;

		breport(entry->size, entry->tag);
		offset += entry->size;
	}

	return allocator->allocated_block;
}

void burst_free_all(burst_allocator* allocator) {
	BX_ASSERT(allocator != NULL && "Invalid arguments passed to burst_free_all");
	for (u32 i = 0; i < darray_length(allocator->entries); ++i) {
		burst_allocator_entry* entry = &allocator->entries[i];
		breport_free(entry->size, entry->tag);
	}

	darray_destroy(allocator->entries);
	allocator->entries = NULL;
	allocator->total_size = 0;
	allocator->allocated_block = NULL;

	platform_free(allocator->allocated_block, FALSE);
}