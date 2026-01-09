#pragma once

#include "defines.h"

typedef enum {
	MEMORY_TAG_UNKNOWN,
	MEMORY_TAG_ENGINE,
	MEMORY_TAG_PLATFORM,
	MEMORY_TAG_CORE,
	MEMORY_TAG_RESOURCES,
	MEMORY_TAG_RENDERER,
	MEMORY_TAG_MAX_TAGS,
} memory_tag;

void memory_shutdown();

void* ballocate(u64 size, memory_tag tag);

void bfree(const void* block, u64 size, memory_tag tag);

void breport(u64 size, memory_tag tag);

void breport_free(u64 size, memory_tag tag);

void* bzero_memory(void* block, u64 size);

void* bcopy_memory(void* dest, const void* source, u64 size);

void* bset_memory(void* dest, i32 value, u64 size);

b8 bcmp_memory(void* buf1, void* buf2, u64 size);

void print_memory_usage();