#pragma once
#include "defines.h"

typedef enum memory_tag {
	MEMORY_TAG_UNKNOWN,
	MEMORY_TAG_ENGINE,
	MEMORY_TAG_PLATFORM,
	MEMORY_TAG_CORE,
	MEMORY_TAG_RESOURCES,
	MEMORY_TAG_RENDERER,
	MEMORY_TAG_MAX_TAGS,
} memory_tag;

b8 memory_initialize();
void memory_shutdown();

void* ballocate(u64 size, memory_tag tag);

void bfree(void* block, u64 size, memory_tag tag);

void print_memory_usage();